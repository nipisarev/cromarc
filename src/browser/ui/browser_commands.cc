// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/browser_commands.h"

#include "cromarc/browser/spaces/space_controller.h"

namespace cromarc {

bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser) {
  return SpaceController::Get()->KeepWindowOnLastTabClose(browser);
}

}  // namespace cromarc
