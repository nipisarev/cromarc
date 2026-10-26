// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_windows.h"

namespace cromarc {

FakeWindows::FakeWindows() = default;
FakeWindows::~FakeWindows() = default;

Profile* FakeWindows::ProfileOf(BrowserWindowInterface* window) {
  auto it = profiles.find(window);
  return it == profiles.end() ? nullptr : it->second;
}

std::vector<BrowserWindowInterface*> FakeWindows::NormalWindowsOf(
    Profile* profile) {
  auto it = normal_windows.find(profile);
  return it == normal_windows.end() ? std::vector<BrowserWindowInterface*>()
                                    : it->second;
}

bool FakeWindows::IsOnScreen(BrowserWindowInterface* window) {
  return !off_screen.contains(window);
}

void FakeWindows::Activate(BrowserWindowInterface* window) {
  activated.push_back(window);
}

}  // namespace cromarc
