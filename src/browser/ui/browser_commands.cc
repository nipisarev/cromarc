// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/browser_commands.h"

#include "chrome/browser/profiles/profile.h"
#include "base/functional/bind.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/glance/hovered_link.h"
#include "cromarc/browser/ui/little/little_window.h"

namespace cromarc {

namespace {

// A Little window of `space`, or of `fallback` when a rule names a space that
// is gone.
void OpenLittleIn(const std::string& space,
                  const std::string& fallback,
                  const GURL& url) {
  SpaceController::Get()->LoadSpace(
      space, base::BindOnce(
                 [](std::string fallback, GURL url, Profile* profile) {
                   if (profile) {
                     LittleWindow::Open(profile, url);
                   } else if (!fallback.empty()) {
                     OpenLittleIn(fallback, std::string(), url);
                   }
                 },
                 fallback, url));
}

void RouteExternalLink(const std::string& fallback, const GURL& url) {
  const std::string space =
      SpaceForExternalLink(SpaceRouting::Get()->routes(), url);
  OpenLittleIn(space.empty() ? fallback : space, fallback, url);
}

}  // namespace

bool OpenExternalLinks(const StartupTabs& tabs, Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }
  for (const StartupTab& tab : tabs) {
    if (tab.is_untrusted_launch) {
      return false;
    }
  }
  // The routing rules choose the space; `profile` (the one used last) takes
  // what they leave.
  for (const StartupTab& tab : tabs) {
    SpaceRouting::Get()->WhenLoaded(base::BindOnce(
        &RouteExternalLink, SpaceController::IdOf(profile), tab.url));
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
