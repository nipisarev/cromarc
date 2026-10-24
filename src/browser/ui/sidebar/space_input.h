// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SPACE_INPUT_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SPACE_INPUT_H_

namespace cromarc {

// Space switching input, Zen's triggers (design, component 4): a two-finger
// horizontal swipe over the sidebar, Ctrl+1…9 / Ctrl+0, Cmd+Alt+← / →, and
// Ctrl + mouse wheel over the sidebar. Installed once, process-wide, ahead of
// Chrome's own event handling, so no upstream accelerator table changes.
void InstallSpaceInputMonitor();

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SPACE_INPUT_H_
