// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
#define CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_

#include <vector>

class BrowserWindowInterface;
class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

class AppLifecycle;
class Tabs;
class Windows;

// Hooks H5, H6 and H10. Each that returns bool returns false when Cromarc does not handle the case;
// the caller then does what Chrome does.

// H5, from AppController: links from other apps open in Little windows of
// `profile` (not for off-the-record profiles; untrusted launches never get
// here).
bool OpenExternalLinks(Tabs& tabs,
                       Windows& windows,
                       AppLifecycle& app_lifecycle,
                       const std::vector<GURL>& urls,
                       Profile* profile);

// H6, from BrowserCommandController (not for popups, apps, off-the-record
// profiles).

// Cmd+T (`edit_current_url` false) and Cmd+L (true): the command bar.
bool ShowCommandBar(Tabs& tabs,
                    Windows& windows,
                    BrowserWindowInterface* browser,
                    bool edit_current_url);

// Cmd+W on the last tab of a space window: the window stays, with a new tab.
bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);

// H10, from BrowserWebContentsDelegate::UpdateTargetURL: the link under the
// cursor in `source`, for Glance.
void OnTargetURL(Tabs& tabs,
                 Windows& windows,
                 content::WebContents* source,
                 const GURL& url);

// Cmd+O in a Little window: the page goes to its space, as Arc's "Open in
// Arc".
bool OpenLittleInSpace(Windows& windows, BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
