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
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "cromarc/browser/lifecycle/policy.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

class ArchiveService;
class TreeService;

// The Arc lifecycle of one regular profile (design, component 3): once a
// minute, on wake from sleep and after session restore, Today tabs idle for
// 12 h go to the archive and pinned items' tabs fall asleep; a tab that could
// lose typed input is deferred and listed under "Needs attention" instead.
// Runs whether or not a sidebar is open.
class LifecycleService : public KeyedService,
                         public base::PowerSuspendObserver {
 public:
  struct TabState {
    raw_ptr<tabs::TabInterface> tab;
    bool deferred = false;
    // Within the last hour before the archive; zero otherwise.
    base::TimeDelta time_left;
  };

  static LifecycleService* Get(Profile* profile);

  LifecycleService(Profile* profile,
                   TreeService* tree_service,
                   ArchiveService* archive_service);
  LifecycleService(const LifecycleService&) = delete;
  LifecycleService& operator=(const LifecycleService&) = delete;
  ~LifecycleService() override;

  // Deferred tabs and countdowns of `browser`'s tabs, for its sidebar.
  std::vector<TabState> GetTabStates(BrowserWindowInterface* browser);
  // "Keep" on a deferred tab: its clock restarts.
  void Keep(tabs::TabInterface* tab);
  // Archives and closes the Today tabs of `browser` (Today's "Clear").
  // Returns the closed URLs, for undo.
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
  TabFacts FactsFor(tabs::TabInterface* tab,
                    bool active_in_visible_window,
                    base::TimeDelta user_away_for,
                    base::Time now);
  bool HasUnsavedInput(tabs::TabInterface* tab);
  void ArchiveAndClose(tabs::TabInterface* tab, const char* reason);
  void Close(tabs::TabInterface* tab);
  // After session restore: every pinned item starts asleep.
  void CloseRestoredPinnedTabs();
  std::vector<BrowserWindowInterface*> Windows();

  raw_ptr<Profile> profile_;
  raw_ptr<TreeService> tree_service_;
  raw_ptr<ArchiveService> archive_service_;
  Limits limits_;
  std::map<tabs::TabHandle, base::Time> last_active_;
  std::set<tabs::TabHandle> deferred_;
  std::set<tabs::TabHandle> kept_;
  bool startup_pass_done_ = false;
  base::RepeatingTimer timer_;
  base::OneShotTimer first_sweep_;
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_H_
