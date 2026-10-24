// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_COMMAND_BAR_CROMARC_COMMAND_BAR_H_
#define CROMARC_BROWSER_UI_COMMAND_BAR_CROMARC_COMMAND_BAR_H_

class BrowserWindowInterface;

namespace cromarc {

inline constexpr char kCommandBarHost[] = "cromarc-command-bar.top-chrome";
inline constexpr char kCommandBarURL[] =
    "chrome://cromarc-command-bar.top-chrome/";

// Hook H6, from BrowserCommandController: Cmd+T (`edit_current_url` false) and
// Cmd+L (true) open the command bar over `browser`. Returns false when the
// window has no command bar (popups, apps, off-the-record profiles); the
// caller then does what Chrome does.
bool ShowCommandBar(BrowserWindowInterface* browser, bool edit_current_url);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_COMMAND_BAR_CROMARC_COMMAND_BAR_H_
