// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_H_
#define CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "cromarc/browser/lifecycle/policy.h"
#include "cromarc/browser/ports/tabs.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

class AppLifecycle;
class ArchiveService;
class TreeService;
class Windows;

// The Arc lifecycle of one regular profile (design, component 3): once a
// minute, on wake from sleep and after session restore, Today tabs idle for
// 12 h go to the archive and pinned items' tabs fall asleep; a tab that could
// lose typed input is deferred and listed under "Needs attention" instead.
// Runs whether or not a sidebar is open.
class LifecycleService : public KeyedService,
                         public base::PowerSuspendObserver {
 public:
  struct TabState {
    TabId tab;
    bool deferred = false;
    // Within the last hour before the archive; zero otherwise.
    base::TimeDelta time_left;
  };

  static LifecycleService* Get(Profile* profile);

  LifecycleService(Profile* profile,
                   TreeService* tree_service,
                   ArchiveService* archive_service,
                   Tabs& tabs,
                   Windows& windows,
                   AppLifecycle& app_lifecycle);
  LifecycleService(const LifecycleService&) = delete;
  LifecycleService& operator=(const LifecycleService&) = delete;
  ~LifecycleService() override;

  // Deferred tabs and countdowns of `browser`'s tabs, for its sidebar.
  std::vector<TabState> GetTabStates(BrowserWindowInterface* browser);
  // "Keep" on a deferred tab: its clock restarts.
  void Keep(TabId tab);
  // Archives and closes the Today tabs of `browser` (Today's "Clear").
  // Returns the archived URLs, for undo.
  std::vector<GURL> ClearToday(BrowserWindowInterface* browser);

  // Runs after every sweep (countdowns move) and after Keep/ClearToday.
  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback);

  void Sweep();

  // KeyedService:
  void Shutdown() override;

  // base::PowerSuspendObserver:
  void OnResume() override;

 private:
  TabFacts FactsFor(TabId tab,
                    bool active_in_visible_window,
                    base::TimeDelta user_away_for,
                    base::Time now);
  bool HasUnsavedInput(TabId tab);
  // Returns the URL it archived the tab under; empty when it had none.
  GURL ArchiveAndClose(TabId tab, const char* reason);
  // After session restore: every pinned item starts asleep.
  void CloseRestoredPinnedTabs();

  raw_ptr<Profile> profile_;
  raw_ptr<TreeService> tree_service_;
  raw_ptr<ArchiveService> archive_service_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
  const raw_ref<AppLifecycle> app_lifecycle_;
  Limits limits_;
  std::map<TabId, base::Time> last_active_;
  std::set<TabId> deferred_;
  std::set<TabId> kept_;
  bool startup_pass_done_ = false;
  base::RepeatingTimer timer_;
  base::OneShotTimer first_sweep_;
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_H_
