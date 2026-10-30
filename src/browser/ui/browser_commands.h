// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
#define CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_

#include <vector>

class BrowserWindowInterface;
class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

class AppLifecycle;
class GlanceRegistry;
class Presence;
class Profiles;
class Tabs;
class Windows;

// Hooks H5, H6 and H10. Each that returns bool returns false when Cromarc does not handle the case;
// the caller then does what Chrome does.

// H5, from AppController: links from other apps open in Little windows of
// `profile` (not for off-the-record profiles; untrusted launches never get
// here).
bool OpenExternalLinks(Profiles& profiles,
                       Tabs& tabs,
                       Windows& windows,
                       AppLifecycle& app_lifecycle,
                       Presence& presence,
                       const std::vector<GURL>& urls,
                       Profile* profile);

// H6, from BrowserCommandController (not for popups, apps, off-the-record
// profiles).

// Cmd+T (`edit_current_url` false) and Cmd+L (true): the command bar.
bool ShowCommandBar(Tabs& tabs,
                    Windows& windows,
                    BrowserWindowInterface* browser,
                    bool edit_current_url);

// Cmd+W on a glance closes the glance; on the last tab of a space window the
// window stays, with a new tab.
bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);

// H10, from BrowserWebContentsDelegate::UpdateTargetURL: the link under the
// cursor in `source`, for Glance.
void OnTargetURL(Tabs& tabs,
                 Windows& windows,
                 GlanceRegistry& glances,
                 content::WebContents* source,
                 const GURL& url);

// Cmd+O on a glance makes it a tab (Zen's zen-glance-expand); in a Little
// window the page goes to its space, as Arc's "Open in Arc".
bool OpenLittleInSpace(Windows& windows, BrowserWindowInterface* browser);

// H21: the Dock icon with no window on screen brings back the space of the
// profile at `profile_path`, as in Zen; false when it is no space.
bool ReopenSpace(const base::FilePath& profile_path);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
