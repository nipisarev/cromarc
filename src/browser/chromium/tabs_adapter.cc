// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/tabs_adapter.h"

#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc

namespace cromarc {

TabsAdapter::TabsAdapter(BrowserWindowInterface* browser) {
  browser->GetTabStripModel()->AddObserver(this);
}

TabsAdapter::~TabsAdapter() = default;

base::CallbackListSubscription TabsAdapter::OnOpenedInBackground(
    base::RepeatingClosure callback) {
  return opened_in_background_.Add(std::move(callback));
}

void TabsAdapter::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted ||
      selection.active_tab_changed()) {
    return;
  }
  opened_in_background_.Notify();
}

}  // namespace cromarc
