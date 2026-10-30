// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/browser_commands.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "url/gurl.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/glance/hovered_link.h"
#include "cromarc/browser/ui/little/little_window.h"

namespace cromarc {

namespace {

// The browser's ports, which outlive every link.
struct LittlePorts {
  raw_ref<Tabs> tabs;
  raw_ref<Windows> windows;
  raw_ref<AppLifecycle> app_lifecycle;
};

// A Little window of `space`, or of `fallback` when a rule names a space that
// is gone.
void OpenLittleIn(LittlePorts ports,
                  const std::string& space,
                  const std::string& fallback,
                  const GURL& url) {
  SpaceController::Get()->LoadSpace(
      space, base::BindOnce(
                 [](LittlePorts ports, std::string fallback, GURL url,
                    Profile* profile) {
                   if (profile) {
                     LittleWindow::Open(*ports.tabs, *ports.windows,
                                        *ports.app_lifecycle, profile, url);
                   } else if (!fallback.empty()) {
                     OpenLittleIn(ports, fallback, std::string(), url);
                   }
                 },
                 ports, fallback, url));
}

void RouteExternalLink(LittlePorts ports,
                       const std::string& fallback,
                       const GURL& url) {
  const std::string space =
      SpaceForExternalLink(SpaceRouting::Get()->routes(), url);
  OpenLittleIn(ports, space.empty() ? fallback : space, fallback, url);
}

}  // namespace

bool OpenExternalLinks(Profiles& profiles,
                       Tabs& tabs,
                       Windows& windows,
                       AppLifecycle& app_lifecycle,
                       const std::vector<GURL>& urls,
                       Profile* profile) {
  if (profiles.IsOffTheRecord(profile)) {
    return false;
  }
  // The routing rules choose the space; `profile` (the one used last) takes
  // what they leave.
  const LittlePorts ports{raw_ref<Tabs>(tabs), raw_ref<Windows>(windows),
                          raw_ref<AppLifecycle>(app_lifecycle)};
  for (const GURL& url : urls) {
    SpaceRouting::Get()->WhenLoaded(base::BindOnce(
        &RouteExternalLink, ports, SpaceController::IdOf(profile), url));
  }
  return true;
}

bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser) {
  return SpaceController::Get()->KeepWindowOnLastTabClose(browser);
}

void OnTargetURL(Tabs& tabs,
                 Windows& windows,
                 content::WebContents* source,
                 const GURL& url) {
  HoveredLink::Set(tabs, windows, source, url);
}

bool ReopenSpace(const base::FilePath& profile_path) {
  // A space's id is its profile's directory name.
  return SpaceController::Get()->Reopen(profile_path.BaseName().AsUTF8Unsafe());
}

bool OpenLittleInSpace(Windows& windows, BrowserWindowInterface* browser) {
  LittleWindow* little = LittleWindow::From(windows, browser);
  if (!little) {
    return false;
  }
  little->OpenInSpace(little->default_space());
  return true;
}

}  // namespace cromarc
