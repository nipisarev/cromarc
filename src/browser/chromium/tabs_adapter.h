// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "cromarc/browser/ports/tabs.h"

class BrowserWindowInterface;

namespace cromarc {

// Tabs on the window's TabStripModel.
class TabsAdapter : public Tabs, public TabStripModelObserver {
 public:
  explicit TabsAdapter(BrowserWindowInterface* browser);
  TabsAdapter(const TabsAdapter&) = delete;
  TabsAdapter& operator=(const TabsAdapter&) = delete;
  ~TabsAdapter() override;

  // Tabs:
  base::CallbackListSubscription OnOpenedInBackground(
      base::RepeatingClosure callback) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  base::RepeatingClosureList opened_in_background_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
