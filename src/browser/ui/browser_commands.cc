// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/browser_commands.h"

#include "chrome/browser/profiles/profile.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/glance/hovered_link.h"
#include "cromarc/browser/ui/little/little_window.h"

namespace cromarc {

bool OpenExternalLinks(const StartupTabs& tabs, Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }
  for (const StartupTab& tab : tabs) {
    if (tab.is_untrusted_launch) {
      return false;
    }
  }
  for (const StartupTab& tab : tabs) {
    LittleWindow::Open(profile, tab.url);
  }
  return true;
}

bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser) {
  return SpaceController::Get()->KeepWindowOnLastTabClose(browser);
}

void OnTargetURL(content::WebContents* source, const GURL& url) {
  HoveredLink::Set(source, url);
}

bool OpenLittleInSpace(BrowserWindowInterface* browser) {
  LittleWindow* little = LittleWindow::From(browser);
  if (!little) {
    return false;
  }
  little->OpenInSpace(little->default_space());
  return true;
}

}  // namespace cromarc
