// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_

#include "cromarc/browser/ports/windows.h"

namespace cromarc {

// Windows on Chrome's BrowserWindowInterface.
class WindowsAdapter : public Windows {
 public:
  WindowsAdapter();
  WindowsAdapter(const WindowsAdapter&) = delete;
  WindowsAdapter& operator=(const WindowsAdapter&) = delete;
  ~WindowsAdapter() override;

  // Windows:
  Profile* ProfileOf(BrowserWindowInterface* window) override;
  std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) override;
  bool IsOnScreen(BrowserWindowInterface* window) override;
  void Activate(BrowserWindowInterface* window) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_
