// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/windows_adapter.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "ui/base/base_window.h"

namespace cromarc {

WindowsAdapter::WindowsAdapter() = default;
WindowsAdapter::~WindowsAdapter() = default;

Profile* WindowsAdapter::ProfileOf(BrowserWindowInterface* window) {
  return window->GetProfile();
}

std::vector<BrowserWindowInterface*> WindowsAdapter::NormalWindowsOf(
    Profile* profile) {
  std::vector<BrowserWindowInterface*> windows;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() == profile &&
        browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
      windows.push_back(browser);
    }
  }
  return windows;
}

bool WindowsAdapter::IsOnScreen(BrowserWindowInterface* window) {
  const ui::BaseWindow* base_window = window->GetWindow();
  return base_window && base_window->IsVisible() &&
         !base_window->IsMinimized();
}

void WindowsAdapter::Activate(BrowserWindowInterface* window) {
  window->GetWindow()->Activate();
}

}  // namespace cromarc
