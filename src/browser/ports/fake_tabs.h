// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_TABS_H_
#define CROMARC_BROWSER_PORTS_FAKE_TABS_H_

#include "base/callback_list.h"
#include "cromarc/browser/ports/tabs.h"

namespace cromarc {

class FakeTabs : public Tabs {
 public:
  FakeTabs();
  ~FakeTabs() override;

  void OpenInBackground();

  // Tabs:
  base::CallbackListSubscription OnOpenedInBackground(
      base::RepeatingClosure callback) override;

 private:
  base::RepeatingClosureList on_opened_in_background_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_TABS_H_
