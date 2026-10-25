// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/compact_hover.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace cromarc {

namespace {

// How far past the revealed sidebar the cursor may wander before it hides.
constexpr int kSlack = 16;
// zen.view.compact.outside-window-edge-offset.horizontal.
constexpr int kOutsideWindowOffset = 200;

}  // namespace

bool CompactSidebarStaysOut(const gfx::Rect& window,
                            const gfx::Rect& strip,
                            const gfx::Point& point) {
  if (window.Contains(point)) {
    return point.y() >= strip.y() && point.y() < strip.bottom() &&
           point.x() <= strip.right() + kSlack;
  }
  // ZenMouseTracker's left edge: beside the window, within its height.
  return point.x() < window.x() &&
         point.x() >= window.x() - kOutsideWindowOffset &&
         point.y() >= window.y() && point.y() <= window.bottom();
}

}  // namespace cromarc
