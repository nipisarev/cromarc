// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/browser_commands.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "url/gurl.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/glance/glance_registry.h"
#include "cromarc/browser/ui/glance/hovered_link.h"
#include "cromarc/browser/ui/little/little_spaces.h"
#include "cromarc/browser/ui/little/little_window.h"

namespace cromarc {

namespace {

// A Little window of `space`, or of `fallback` when a rule names a space that
// is gone.
void OpenLittleIn(LittleWindow::Deps deps,
                  const std::string& space,
                  const std::string& fallback,
                  const GURL& url) {
  deps.delegate->LoadSpace(
      space, base::BindOnce(
                 [](LittleWindow::Deps deps, std::string fallback, GURL url,
                    Profile* profile) {
                   if (profile) {
                     LittleWindow::Open(deps, profile, url);
                   } else if (!fallback.empty()) {
                     OpenLittleIn(deps, fallback, std::string(), url);
                   }
                 },
                 deps, fallback, url));
}

void RouteExternalLink(LittleWindow::Deps deps,
                       const std::string& fallback,
                       const GURL& url) {
  const std::string space =
      SpaceForExternalLink(SpaceRouting::Get()->routes(), url);
  OpenLittleIn(deps, space.empty() ? fallback : space, fallback, url);
}

}  // namespace

bool OpenExternalLinks(Profiles& profiles,
                       Tabs& tabs,
                       Windows& windows,
                       AppLifecycle& app_lifecycle,
                       Presence& presence,
                       const std::vector<GURL>& urls,
                       Profile* profile) {
  if (profiles.IsOffTheRecord(profile)) {
    return false;
  }
  static base::NoDestructor<LittleSpaces> spaces;
  const LittleWindow::Deps deps{
      raw_ref<Tabs>(tabs),
      raw_ref<Windows>(windows),
      raw_ref<Profiles>(profiles),
      raw_ref<AppLifecycle>(app_lifecycle),
      raw_ref<Presence>(presence),
      raw_ref<LittleWindow::Delegate>(*spaces)};
  // The routing rules choose the space; `profile` (the one used last) takes
  // what they leave.
  for (const GURL& url : urls) {
    SpaceRouting::Get()->WhenLoaded(base::BindOnce(
        &RouteExternalLink, deps, profiles.IdOf(profile), url));
  }
  return true;
}

bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser) {
  // Cmd+W on a glance closes only the glance (Zen's manageTabClose).
  if (GlanceRegistry::Get()->CloseActive(browser)) {
    return true;
  }
  return SpaceController::Get()->KeepWindowOnLastTabClose(browser);
}

void OnTargetURL(Tabs& tabs,
                 Windows& windows,
                 GlanceRegistry& glances,
                 content::WebContents* source,
                 const GURL& url) {
  HoveredLink::Set(tabs, windows, glances, source, url);
}

bool ReopenSpace(const base::FilePath& profile_path) {
  // A space's id is its profile's directory name.
  return SpaceController::Get()->Reopen(profile_path.BaseName().AsUTF8Unsafe());
}

bool OpenLittleInSpace(Windows& windows, BrowserWindowInterface* browser) {
  // Zen's Cmd+O (zen-glance-expand): a glance becomes a tab.
  if (GlanceRegistry::Get()->ExpandActive(browser)) {
    return true;
  }
  LittleWindow* little = LittleWindow::From(windows, browser);
  if (!little) {
    return false;
  }
  little->OpenInSpace(little->default_space());
  return true;
}

}  // namespace cromarc
