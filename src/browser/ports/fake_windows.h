// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_
#define CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "cromarc/browser/ports/windows.h"

namespace cromarc {

// Windows and profiles here are handles that point at nothing; a test makes
// them with Handle<BrowserWindowInterface>(n) and Handle<Profile>(n).
class FakeWindows : public Windows {
 public:
  FakeWindows();
  ~FakeWindows() override;

  template <typename T>
  static T* Handle(int n) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(0x1000 * n));
  }

  // Windows:
  Profile* ProfileOf(BrowserWindowInterface* window) override;
  std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) override;
  bool IsOnScreen(BrowserWindowInterface* window) override;
  void Activate(BrowserWindowInterface* window) override;

  std::map<BrowserWindowInterface*, Profile*> profiles;
  std::map<Profile*, std::vector<BrowserWindowInterface*>> normal_windows;
  std::set<BrowserWindowInterface*> off_screen;
  std::vector<BrowserWindowInterface*> activated;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_
