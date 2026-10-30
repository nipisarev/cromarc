// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/lifecycle_service.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/app_lifecycle.h"
#include "cromarc/browser/ports/presence.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/glance/glance_registry.h"

namespace cromarc {

namespace {

constexpr base::TimeDelta kSweepInterval = base::Minutes(1);
// Lets session restore and the first window settle before the first sweep.
constexpr base::TimeDelta kFirstSweepDelay = base::Seconds(10);

}  // namespace

LifecycleService::LifecycleService(Profile* profile,
                                   TreeService* tree_service,
                                   ArchiveService* archive_service,
                                   Tabs& tabs,
                                   Windows& windows,
                                   AppLifecycle& app_lifecycle,
                                   Presence& presence,
                                   GlanceRegistry& glances)
    : profile_(profile),
      tree_service_(tree_service),
      archive_service_(archive_service),
      tabs_(tabs),
      windows_(windows),
      app_lifecycle_(app_lifecycle),
      presence_(presence),
      glances_(glances),
      limits_(LimitsFromCommandLine()) {
  const base::TimeDelta interval =
      std::clamp(limits_.idle / 6, base::Seconds(1), kSweepInterval);
  timer_.Start(FROM_HERE, interval,
               base::BindRepeating(&LifecycleService::Sweep,
                                   base::Unretained(this)));
  first_sweep_.Start(FROM_HERE, kFirstSweepDelay,
                     base::BindOnce(&LifecycleService::Sweep,
                                    base::Unretained(this)));
  if (base::PowerMonitor* monitor = base::PowerMonitor::GetInstance()) {
    monitor->AddPowerSuspendObserver(this);
  }
}

LifecycleService::~LifecycleService() = default;

void LifecycleService::Shutdown() {
  timer_.Stop();
  first_sweep_.Stop();
  if (base::PowerMonitor* monitor = base::PowerMonitor::GetInstance()) {
    monitor->RemovePowerSuspendObserver(this);
  }
}

void LifecycleService::OnResume() {
  Sweep();
}

base::CallbackListSubscription LifecycleService::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

bool LifecycleService::HasUnsavedInput(TabId tab) {
  // A tab and its glance close together: input in either is at stake.
  const std::optional<TabId> glance = GlanceOf(tab);
  return tabs_->MayLoseInput(tab) || (glance && tabs_->MayLoseInput(*glance));
}

bool LifecycleService::IsGlance(TabId tab) const {
  return glances_->IsGlance(tab);
}

std::optional<TabId> LifecycleService::GlanceOf(TabId tab) const {
  return glances_->GlanceOf(tab);
}

bool LifecycleService::Shows(TabId tab, std::optional<TabId> active) const {
  return active && (tab == *active || GlanceOf(tab) == active);
}

TabFacts LifecycleService::FactsFor(TabId tab,
                                    bool active_in_visible_window,
                                    base::TimeDelta user_away_for,
                                    base::Time now) {
  auto it = last_active_.find(tab);
  if (it == last_active_.end()) {
    // First sight: when the tab was last shown (restored tabs keep the time
    // from the previous session).
    it = last_active_.emplace(tab, tabs_->GetLastActiveTime(tab)).first;
  }

  TabFacts facts;
  facts.pinned_item = tree_service_->GetItemForTab(tab).has_value();
  facts.active_in_visible_window = active_in_visible_window;
  facts.user_away_for = user_away_for;
  facts.paused = tabs_->IsAudibleOrCaptured(tab);
  facts.kept_by_user = kept_.contains(tab);
  if ((active_in_visible_window && user_away_for < limits_.away) ||
      facts.paused) {
    it->second = now;
  }
  facts.last_active = it->second;
  return facts;
}

void LifecycleService::Sweep() {
  if (app_lifecycle_->IsRestoringSession(profile_)) {
    return;
  }
  if (!startup_pass_done_) {
    startup_pass_done_ = true;
    CloseRestoredPinnedTabs();
  }

  const base::Time now = base::Time::Now();
  base::TimeDelta away = presence_->IdleFor();
  if (presence_->IsScreenLocked()) {
    away = std::max(away, limits_.away);
  }

  std::set<TabId> seen;
  for (BrowserWindowInterface* browser : windows_->NormalWindowsOf(profile_)) {
    const bool visible = windows_->IsOnScreen(browser);
    const std::optional<TabId> active = tabs_->GetActive(browser);
    const std::vector<TabId> tabs = tabs_->List(browser);
    // Counted here, as a tab with unload listeners leaves the strip only
    // later. A glance is no page the window keeps: it goes with its tab.
    size_t open = std::ranges::count_if(
        tabs, [this](TabId tab) { return !IsGlance(tab); });
    for (TabId tab : tabs) {
      seen.insert(tab);
      if (IsGlance(tab)) {
        continue;
      }
      TabFacts facts = FactsFor(tab, visible && Shows(tab, active), away, now);
      Decision decision = Decide(facts, now, limits_);
      if (decision != Decision::kKeep) {
        facts.unsaved_input = HasUnsavedInput(tab);
        decision = Decide(facts, now, limits_);
      }
      // A window keeps its last tab: an empty space window has no page.
      if ((decision == Decision::kArchive || decision == Decision::kSleep) &&
          open <= 1) {
        decision = Decision::kKeep;
      }
      switch (decision) {
        case Decision::kKeep:
          deferred_.erase(tab);
          break;
        case Decision::kDefer:
          deferred_.insert(tab);
          break;
        case Decision::kArchive:
          ArchiveAndClose(tab, "idle");
          --open;
          break;
        case Decision::kSleep:
          tabs_->Close(tab);
          --open;
          break;
      }
    }
  }

  // Forget closed tabs.
  std::erase_if(last_active_,
                [&seen](const auto& entry) { return !seen.contains(entry.first); });
  std::erase_if(deferred_, [&seen](TabId tab) { return !seen.contains(tab); });
  std::erase_if(kept_, [&seen](TabId tab) { return !seen.contains(tab); });
  // Countdowns move every minute, so sidebars refresh after every sweep.
  on_changed_.Notify();
}

void LifecycleService::CloseRestoredPinnedTabs() {
  for (BrowserWindowInterface* browser : windows_->NormalWindowsOf(profile_)) {
    const std::vector<TabId> tabs = tabs_->List(browser);
    // Counted here, as a tab with unload listeners leaves the strip only
    // later: the window keeps a page.
    size_t open = tabs.size();
    for (auto it = tabs.rbegin(); it != tabs.rend() && open > 1; ++it) {
      if (tabs_->IsPinned(*it) && !tree_service_->GetItemForTab(*it)) {
        tabs_->Close(*it);
        --open;
      }
    }
  }
}

GURL LifecycleService::ArchiveAndClose(TabId tab, const char* reason) {
  ArchiveEntry entry;
  // A page still loading has committed nothing: it goes under the address
  // it is loading, the one its row shows.
  entry.url = tabs_->GetLastCommittedUrl(tab);
  if (entry.url.is_empty()) {
    entry.url = tabs_->GetUrl(tab);
  }
  entry.title = tabs_->GetTitle(tab);
  entry.closed_at = base::Time::Now();
  entry.reason = reason;
  GURL archived;
  if (entry.url.is_valid()) {
    archived = entry.url;
    archive_service_->Add(std::move(entry));
  }
  tabs_->Close(tab);
  return archived;
}

std::vector<LifecycleService::TabState> LifecycleService::GetTabStates(
    BrowserWindowInterface* browser) {
  std::vector<TabState> states;
  const base::Time now = base::Time::Now();
  base::TimeDelta away = presence_->IdleFor();
  const std::optional<TabId> active = tabs_->GetActive(browser);
  const bool visible = windows_->IsOnScreen(browser);
  for (TabId tab : tabs_->List(browser)) {
    if (IsGlance(tab)) {
      continue;
    }
    TabFacts facts = FactsFor(tab, visible && Shows(tab, active), away, now);
    TabState state;
    state.tab = tab;
    state.deferred = deferred_.contains(tab);
    state.time_left = TimeToArchive(facts, now, limits_);
    if (state.deferred || state.time_left.is_positive()) {
      states.push_back(state);
    }
  }
  return states;
}

void LifecycleService::Keep(TabId tab) {
  deferred_.erase(tab);
  kept_.insert(tab);
  last_active_[tab] = base::Time::Now();
  on_changed_.Notify();
}

std::vector<GURL> LifecycleService::ClearToday(
    BrowserWindowInterface* browser) {
  std::vector<TabId> tabs = tabs_->List(browser);
  // Glances go with their tabs.
  std::erase_if(tabs, [this](TabId tab) { return IsGlance(tab); });
  std::vector<TabId> today;
  for (TabId tab : tabs) {
    if (!tree_service_->GetItemForTab(tab)) {
      today.push_back(tab);
    }
  }
  // The window keeps a page: with nothing but Today in it, the last tab
  // stays. Counted before closing, as a tab with unload listeners leaves the
  // strip only later.
  if (!today.empty() && today.size() == tabs.size()) {
    today.pop_back();
  }
  std::vector<GURL> urls;
  for (TabId tab : today) {
    GURL url = ArchiveAndClose(tab, "clear");
    if (url.is_valid()) {
      urls.push_back(std::move(url));
    }
  }
  on_changed_.Notify();
  return urls;
}

}  // namespace cromarc
