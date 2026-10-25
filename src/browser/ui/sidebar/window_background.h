// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_WINDOW_BACKGROUND_H_
#define CROMARC_BROWSER_UI_SIDEBAR_WINDOW_BACKGROUND_H_

class BrowserWindowInterface;

namespace cromarc {

// Paints the window's background again: its space theme or its preview
// changed.
void RepaintWindowBackground(BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_WINDOW_BACKGROUND_H_
