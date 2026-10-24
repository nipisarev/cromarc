// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
#define CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_

class BrowserWindowInterface;

namespace cromarc {

// Hook H6, from BrowserCommandController. Each returns false when Cromarc does
// not handle the command for `browser` (popups, apps, off-the-record
// profiles); the caller then does what Chrome does.

// Cmd+T (`edit_current_url` false) and Cmd+L (true): the command bar.
bool ShowCommandBar(BrowserWindowInterface* browser, bool edit_current_url);

// Cmd+W on the last tab of a space window: the window stays, with a new tab.
bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_BROWSER_COMMANDS_H_
