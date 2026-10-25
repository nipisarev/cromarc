// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_COMPACT_HOVER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_COMPACT_HOVER_H_

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace cromarc {

// Whether compact mode's revealed sidebar stays out with the cursor at
// `point` (screen coordinates): over the sidebar or just past it in the
// window, or out of the window past the sidebar's edge as far as Zen's mouse
// tracker allows (zen:compact-mode/ZenMouseTracker.cpp).
bool CompactSidebarStaysOut(const gfx::Rect& window,
                            const gfx::Rect& strip,
                            const gfx::Point& point);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_COMPACT_HOVER_H_
