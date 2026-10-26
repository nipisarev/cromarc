// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_DEFAULT_BROWSER_H_
#define CROMARC_BROWSER_PORTS_DEFAULT_BROWSER_H_

#include "base/functional/callback_forward.h"

namespace cromarc {

// Whether Cromarc is the system's default browser.
class DefaultBrowser {
 public:
  virtual ~DefaultBrowser() = default;

  virtual void CheckIsDefault(base::OnceCallback<void(bool)> done) = 0;
  // Asks the system to make Cromarc the default; `done` says whether it is.
  virtual void SetAsDefault(base::OnceCallback<void(bool)> done) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_DEFAULT_BROWSER_H_
