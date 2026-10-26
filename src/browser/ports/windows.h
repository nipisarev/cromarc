// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_WINDOWS_H_
#define CROMARC_BROWSER_PORTS_WINDOWS_H_

#include <vector>

class BrowserWindowInterface;
class Profile;

namespace cromarc {

// The browser's windows. Windows (BrowserWindowInterface) and profiles are
// pointers used as handles only.
class Windows {
 public:
  virtual ~Windows() = default;

  virtual Profile* ProfileOf(BrowserWindowInterface* window) = 0;
  // `profile`'s normal windows, the ones with a sidebar.
  virtual std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) = 0;
  // Shown and not minimized.
  virtual bool IsOnScreen(BrowserWindowInterface* window) = 0;
  virtual void Activate(BrowserWindowInterface* window) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_WINDOWS_H_
