// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/compact_hover.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace cromarc {
namespace {

// A window at (22, 64), its revealed sidebar 247 wide under the toolbar.
const gfx::Rect kWindow(22, 64, 1100, 800);
const gfx::Rect kStrip(22, 100, 247, 764);

TEST(CompactHoverTest, OverTheSidebarItStays) {
  EXPECT_TRUE(CompactSidebarStaysOut(kWindow, kStrip, {100, 400}));
  // A little past its edge too.
  EXPECT_TRUE(CompactSidebarStaysOut(kWindow, kStrip, {22 + 247 + 16, 400}));
}

TEST(CompactHoverTest, OverThePageItGoes) {
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {600, 400}));
  // Above the sidebar, in the toolbar.
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {100, 80}));
}

TEST(CompactHoverTest, PastTheLeftEdgeOfTheWindowItStays) {
  EXPECT_TRUE(CompactSidebarStaysOut(kWindow, kStrip, {10, 400}));
  EXPECT_TRUE(CompactSidebarStaysOut(kWindow, kStrip, {22 - 200, 400}));
}

TEST(CompactHoverTest, ElsewhereOutOfTheWindowItGoes) {
  // Further left than Zen's 200 points.
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {22 - 201, 400}));
  // Left of the window but above or below it.
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {10, 20}));
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {10, 900}));
  // Right of or below the window.
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {1300, 400}));
  EXPECT_FALSE(CompactSidebarStaysOut(kWindow, kStrip, {100, 1000}));
}

}  // namespace
}  // namespace cromarc
