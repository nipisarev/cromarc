// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_TABS_H_
#define CROMARC_BROWSER_PORTS_TABS_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

namespace cromarc {

// One window's tabs.
class Tabs {
 public:
  virtual ~Tabs() = default;

  // Called once per insertion of tabs that leaves the active tab as it was.
  virtual base::CallbackListSubscription OnOpenedInBackground(
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_TABS_H_
