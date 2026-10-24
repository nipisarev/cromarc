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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/lifecycle/lifecycle_service_factory.h"
#include "cromarc/browser/tree/tree_service.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#include "ui/base/base_window.h"
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

bool IsVisible(BrowserWindowInterface* browser) {
  const ui::BaseWindow* window = browser->GetWindow();
  return window && window->IsVisible() && !window->IsMinimized();
}

}  // namespace

// static
LifecycleService* LifecycleService::Get(Profile* profile) {
  return LifecycleServiceFactory::GetForProfile(profile);
}

LifecycleService::LifecycleService(Profile* profile,
                                   TreeService* tree_service,
                                   ArchiveService* archive_service)
    : profile_(profile),
      tree_service_(tree_service),
      archive_service_(archive_service),
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

std::vector<BrowserWindowInterface*> LifecycleService::Windows() {
  std::vector<BrowserWindowInterface*> windows;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() == profile_ &&
        browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
      windows.push_back(browser);
    }
  }
  return windows;
}

bool LifecycleService::HasUnsavedInput(tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  bool has_handler = false;
  contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&has_handler](content::RenderFrameHost* frame) {
        if (frame->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::
                    kBeforeUnloadHandler)) {
          has_handler = true;
        }
      });
  if (!has_handler) {
    return false;
  }
  if (!performance_manager::PerformanceManager::IsAvailable()) {
    return true;
  }
  base::WeakPtr<performance_manager::PageNode> page =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          contents);
  return !page || page->HadUserEdits();
}

TabFacts LifecycleService::FactsFor(tabs::TabInterface* tab,
                                    bool active_in_visible_window,
                                    base::TimeDelta user_away_for,
                                    base::Time now) {
  content::WebContents* contents = tab->GetContents();
  const tabs::TabHandle handle = tab->GetHandle();
  auto it = last_active_.find(handle);
  if (it == last_active_.end()) {
    // First sight: when the tab was last shown (restored tabs keep the time
    // from the previous session).
    it = last_active_.emplace(handle, contents->GetLastActiveTime()).first;
  }

  TabFacts facts;
  facts.pinned_item = tree_service_->GetItemForTab(tab).has_value();
  facts.active_in_visible_window = active_in_visible_window;
  facts.user_away_for = user_away_for;
  facts.paused = contents->IsCurrentlyAudible() || contents->IsBeingCaptured();
  facts.kept_by_user = kept_.contains(handle);
  if ((active_in_visible_window && user_away_for < limits_.away) ||
      facts.paused) {
    it->second = now;
  }
  facts.last_active = it->second;
  return facts;
}

void LifecycleService::Sweep() {
  if (SessionRestore::IsRestoring(profile_)) {
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

  std::set<tabs::TabHandle> seen;
  for (BrowserWindowInterface* browser : Windows()) {
    TabStripModel* model = browser->GetTabStripModel();
    const bool visible = IsVisible(browser);
    std::vector<tabs::TabInterface*> tabs;
    for (int i = 0; i < model->count(); ++i) {
      tabs.push_back(model->GetTabAtIndex(i));
    }
    tabs::TabInterface* active = model->GetTabAtIndex(model->active_index());
    for (tabs::TabInterface* tab : tabs) {
      const tabs::TabHandle handle = tab->GetHandle();
      seen.insert(handle);
      TabFacts facts = FactsFor(tab, visible && tab == active, away, now);
      Decision decision = Decide(facts, now, limits_);
      if (decision != Decision::kKeep) {
        facts.unsaved_input = HasUnsavedInput(tab);
        decision = Decide(facts, now, limits_);
      }
      // A window keeps its last tab: an empty space window has no page.
      if ((decision == Decision::kArchive || decision == Decision::kSleep) &&
          model->count() <= 1) {
        decision = Decision::kKeep;
      }
      switch (decision) {
        case Decision::kKeep:
          deferred_.erase(handle);
          break;
        case Decision::kDefer:
          deferred_.insert(handle);
          break;
        case Decision::kArchive:
          ArchiveAndClose(tab, "idle");
          break;
        case Decision::kSleep:
          Close(tab);
          break;
      }
    }
  }

  // Forget closed tabs.
  std::erase_if(last_active_,
                [&seen](const auto& entry) { return !seen.contains(entry.first); });
  std::erase_if(deferred_,
                [&seen](const tabs::TabHandle& h) { return !seen.contains(h); });
  std::erase_if(kept_,
                [&seen](const tabs::TabHandle& h) { return !seen.contains(h); });
  // Countdowns move every minute, so sidebars refresh after every sweep.
  on_changed_.Notify();
}

void LifecycleService::CloseRestoredPinnedTabs() {
  for (BrowserWindowInterface* browser : Windows()) {
    TabStripModel* model = browser->GetTabStripModel();
    for (int i = model->count() - 1; i >= 0; --i) {
      tabs::TabInterface* tab = model->GetTabAtIndex(i);
      if (model->IsTabPinned(i) && !tree_service_->GetItemForTab(tab) &&
          model->count() > 1) {
        Close(tab);
      }
    }
  }
}

void LifecycleService::ArchiveAndClose(tabs::TabInterface* tab,
                                       const char* reason) {
  content::WebContents* contents = tab->GetContents();
  ArchiveEntry entry;
  entry.url = contents->GetLastCommittedURL();
  entry.title = contents->GetTitle();
  entry.closed_at = base::Time::Now();
  entry.reason = reason;
  if (entry.url.is_valid() && !entry.url.is_empty()) {
    archive_service_->Add(std::move(entry));
  }
  Close(tab);
}

void LifecycleService::Close(tabs::TabInterface* tab) {
  TabStripModel* model = tab->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(tab);
  if (index != TabStripModel::kNoTab) {
    model->CloseWebContentsAt(index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

std::vector<LifecycleService::TabState> LifecycleService::GetTabStates(
    BrowserWindowInterface* browser) {
  std::vector<TabState> states;
  const base::Time now = base::Time::Now();
  base::TimeDelta away = base::Seconds(ui::CalculateIdleTime());
  TabStripModel* model = browser->GetTabStripModel();
  tabs::TabInterface* active = model->GetTabAtIndex(model->active_index());
  const bool visible = IsVisible(browser);
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    TabFacts facts = FactsFor(tab, visible && tab == active, away, now);
    TabState state;
    state.tab = tab;
    state.deferred = deferred_.contains(tab->GetHandle());
    state.time_left = TimeToArchive(facts, now, limits_);
    if (state.deferred || state.time_left.is_positive()) {
      states.push_back(state);
    }
  }
  return states;
}

void LifecycleService::Keep(tabs::TabInterface* tab) {
  const tabs::TabHandle handle = tab->GetHandle();
  deferred_.erase(handle);
  kept_.insert(handle);
  last_active_[handle] = base::Time::Now();
  on_changed_.Notify();
}

std::vector<GURL> LifecycleService::ClearToday(
    BrowserWindowInterface* browser) {
  TabStripModel* model = browser->GetTabStripModel();
  std::vector<tabs::TabInterface*> today;
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    if (!tree_service_->GetItemForTab(tab)) {
      today.push_back(tab);
    }
  }
  std::vector<GURL> urls;
  for (tabs::TabInterface* tab : today) {
    // The window keeps a page: the last tab stays.
    if (model->count() <= 1) {
      break;
    }
    urls.push_back(tab->GetContents()->GetLastCommittedURL());
    ArchiveAndClose(tab, "clear");
  }
  on_changed_.Notify();
  return urls;
}

}  // namespace cromarc
