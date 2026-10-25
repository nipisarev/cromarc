// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SPACE_WINDOW_H_
#define CROMARC_BROWSER_SPACES_SPACE_WINDOW_H_

class BrowserWindowInterface;

namespace cromarc {

// A space window brought over the visible one ahead of a switch: at 1%
// opacity, which Chrome still counts as on screen (so its pages paint) and
// as not hiding the window beneath (alpha below 1, mouse let through:
// WebContentsOcclusionCheckerMac windowCanOccludeOtherWindows). False puts it
// back to normal.
void SetWindowFaint(BrowserWindowInterface* browser, bool faint);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SPACE_WINDOW_H_
