// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The composition root of the space controller: the browser-process
// singleton on the Chromium ports.

#include "base/no_destructor.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/spaces/space_controller.h"

namespace cromarc {

// static
SpaceController* SpaceController::Get() {
  static base::NoDestructor<SpaceController> instance(
      ChromiumProfiles(), ChromiumWindows(), ChromiumTabs(), ChromiumTheme(),
      ChromiumStripDefaults());
  return instance.get();
}

// static
std::string SpaceController::IdOf(Profile* profile) {
  return ChromiumProfiles().IdOf(profile);
}

// static
BrowserWindowInterface* SpaceController::GetWindowFor(Profile* profile) {
  return Get()->WindowFor(profile);
}

}  // namespace cromarc
