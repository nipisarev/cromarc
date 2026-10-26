// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/lifecycle_service.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/power_monitor/power_monitor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/app_lifecycle.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/tree/tree_service.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#include "ui/base/idle/idle.h"

namespace cromarc {

namespace {

constexpr base::TimeDelta kSweepInterval = base::Minutes(1);
// Lets session restore and the first window settle before the first sweep.
constexpr base::TimeDelta kFirstSweepDelay = base::Seconds(10);

// --cromarc-idle-limit=SECONDS: a short idle limit to try the lifecycle by
// hand or in a test; sweeps run more often to match.
constexpr char kIdleLimitSwitch[] = "cromarc-idle-limit";

Limits LimitsFromCommandLine() {
  Limits limits;
  int seconds = 0;
  if (base::StringToInt(base::CommandLine::ForCurrentProcess()
                            ->GetSwitchValueASCII(kIdleLimitSwitch),
                        &seconds) &&
      seconds > 0) {
    limits.idle = base::Seconds(seconds);
    limits.show_time_left_within = limits.idle / 2;
  }
  return limits;
}

}  // namespace

LifecycleService::LifecycleService(Profile* profile,
                                   TreeService* tree_service,
                                   ArchiveService* archive_service,
                                   Tabs& tabs,
                                   Windows& windows,
                                   AppLifecycle& app_lifecycle)
    : profile_(profile),
      tree_service_(tree_service),
      archive_service_(archive_service),
      tabs_(tabs),
      windows_(windows),
      app_lifecycle_(app_lifecycle),
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
  content::WebContents* contents = tabs_->GetContents(tab);
  bool has_handler = false;
  contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&has_handler](content::RenderFrameHost* frame) {
        if (frame->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::
                    kBeforeUnloadHandler)) {
          has_handler = true;
        }
      });
  return has_handler && app_lifecycle_->MayHaveUserEdits(contents);
}

TabFacts LifecycleService::FactsFor(TabId tab,
                                    bool active_in_visible_window,
                                    base::TimeDelta user_away_for,
                                    base::Time now) {
  content::WebContents* contents = tabs_->GetContents(tab);
  auto it = last_active_.find(tab);
  if (it == last_active_.end()) {
    // First sight: when the tab was last shown (restored tabs keep the time
    // from the previous session).
    it = last_active_.emplace(tab, contents->GetLastActiveTime()).first;
  }

  TabFacts facts;
  facts.pinned_item = tree_service_->GetItemForTab(tab).has_value();
  facts.active_in_visible_window = active_in_visible_window;
  facts.user_away_for = user_away_for;
  facts.paused = contents->IsCurrentlyAudible() || contents->IsBeingCaptured();
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
  base::TimeDelta away = base::Seconds(ui::CalculateIdleTime());
  if (ui::CheckIdleStateIsLocked()) {
    away = std::max(away, limits_.away);
  }

  std::set<TabId> seen;
  for (BrowserWindowInterface* browser : windows_->NormalWindowsOf(profile_)) {
    const bool visible = windows_->IsOnScreen(browser);
    const std::optional<TabId> active = tabs_->GetActive(browser);
    for (TabId tab : tabs_->List(browser)) {
      seen.insert(tab);
      TabFacts facts = FactsFor(tab, visible && tab == active, away, now);
      Decision decision = Decide(facts, now, limits_);
      if (decision != Decision::kKeep) {
        facts.unsaved_input = HasUnsavedInput(tab);
        decision = Decide(facts, now, limits_);
      }
      // A window keeps its last tab: an empty space window has no page.
      if ((decision == Decision::kArchive || decision == Decision::kSleep) &&
          tabs_->List(browser).size() <= 1) {
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
          break;
        case Decision::kSleep:
          tabs_->Close(tab);
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
    for (auto it = tabs.rbegin(); it != tabs.rend(); ++it) {
      if (tabs_->IsPinned(*it) && !tree_service_->GetItemForTab(*it) &&
          tabs_->List(browser).size() > 1) {
        tabs_->Close(*it);
      }
    }
  }
}

GURL LifecycleService::ArchiveAndClose(TabId tab, const char* reason) {
  content::WebContents* contents = tabs_->GetContents(tab);
  ArchiveEntry entry;
  // A page still loading has committed nothing: it goes under the address
  // it is loading, the one its row shows.
  entry.url = contents->GetLastCommittedURL().is_empty()
                  ? contents->GetVisibleURL()
                  : contents->GetLastCommittedURL();
  entry.title = contents->GetTitle();
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
  base::TimeDelta away = base::Seconds(ui::CalculateIdleTime());
  const std::optional<TabId> active = tabs_->GetActive(browser);
  const bool visible = windows_->IsOnScreen(browser);
  for (TabId tab : tabs_->List(browser)) {
    TabFacts facts = FactsFor(tab, visible && tab == active, away, now);
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
  const std::vector<TabId> tabs = tabs_->List(browser);
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
