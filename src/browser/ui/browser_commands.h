// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
#define CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_

#include "chrome/browser/ui/startup/startup_tab.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

// Hooks H5 and H6. Each returns false when Cromarc does not handle the case;
// the caller then does what Chrome does.

// H5, from AppController: links from other apps open in Little windows of
// `profile` (not for off-the-record profiles or untrusted launches).
bool OpenExternalLinks(const StartupTabs& tabs, Profile* profile);

// H6, from BrowserCommandController (not for popups, apps, off-the-record
// profiles).

// Cmd+T (`edit_current_url` false) and Cmd+L (true): the command bar.
bool ShowCommandBar(BrowserWindowInterface* browser, bool edit_current_url);

// Cmd+W on the last tab of a space window: the window stays, with a new tab.
bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);

// Cmd+O in a Little window: the page goes to its space, as Arc's "Open in
// Arc".
bool OpenLittleInSpace(BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
