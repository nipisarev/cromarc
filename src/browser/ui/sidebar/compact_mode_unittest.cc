// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/compact_mode.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "cromarc/browser/ports/fake_browser_frame.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_vertical_strip.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace cromarc {
namespace {

// The window is at (0, 0), 1100 x 800; the strip under its toolbar, from
// y 40 down (FakeBrowserFrame, FakeVerticalStrip).
constexpr gfx::Point kAtTheEdge(3, 400);
constexpr gfx::Point kOverThePage(600, 400);
constexpr gfx::Point kOverTheSidebar(100, 400);

class CompactModeTest : public testing::Test {
 protected:
  base::ScopedClosureRunner WatchMouse(
      base::RepeatingCallback<void(const gfx::Point&)> on_move) {
    ++watching_;
    return base::ScopedClosureRunner(base::BindOnce(
        [](int* watching) { --*watching; }, base::Unretained(&watching_)));
  }

  void Compact() { strip_.collapsed = true; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeVerticalStrip strip_;
  FakeBrowserFrame frame_;
  FakeTabs tabs_;
  int watching_ = 0;
  CompactMode compact_{strip_, frame_, tabs_,
                       base::BindRepeating(&CompactModeTest::WatchMouse,
                                           base::Unretained(this))};
};

TEST_F(CompactModeTest, ToggleCollapsesAndExpands) {
  EXPECT_TRUE(compact_.Toggle());
  EXPECT_TRUE(compact_.IsCompact());
  EXPECT_TRUE(compact_.Toggle());
  EXPECT_FALSE(compact_.IsCompact());
  EXPECT_EQ((std::vector<bool>{true, false}), strip_.collapse_requests);
}

TEST_F(CompactModeTest, ToggleWhileCollapsingTurnsBack) {
  strip_.collapsing = true;
  compact_.Toggle();
  EXPECT_EQ(std::vector<bool>{false}, strip_.collapse_requests);
}

TEST_F(CompactModeTest, NoVerticalTabsNoToggle) {
  strip_.shown = false;
  EXPECT_FALSE(compact_.Toggle());
  EXPECT_TRUE(strip_.collapse_requests.empty());
}

TEST_F(CompactModeTest, TheLeftEdgeRevealsTheSidebar) {
  Compact();
  compact_.OnMouseMoved(kAtTheEdge);
  EXPECT_EQ(1, strip_.holds);
  EXPECT_EQ(1, watching_);
}

TEST_F(CompactModeTest, OnlyTheEdgeBesideTheStripRevealsIt) {
  Compact();
  compact_.OnMouseMoved({20, 400});
  // In the toolbar, above the strip.
  compact_.OnMouseMoved({3, 20});
  EXPECT_EQ(0, strip_.holds);
}

TEST_F(CompactModeTest, NotCompactNothingSlidesOut) {
  compact_.OnMouseMoved(kAtTheEdge);
  tabs_.OpenInBackground();
  EXPECT_EQ(0, strip_.holds);
}

TEST_F(CompactModeTest, AStripOutOfItsWindowStaysIn) {
  Compact();
  strip_.bounds.reset();
  compact_.OnMouseMoved(kAtTheEdge);
  EXPECT_EQ(0, strip_.holds);
  EXPECT_EQ(0, watching_);
}

TEST_F(CompactModeTest, LeavingHidesItAfter150Ms) {
  Compact();
  compact_.OnMouseMoved(kAtTheEdge);
  compact_.OnMouseMoved(kOverThePage);
  task_environment_.FastForwardBy(base::Milliseconds(149));
  EXPECT_EQ(1, strip_.holds);
  task_environment_.FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0, strip_.holds);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, watching_);
}

TEST_F(CompactModeTest, ComingBackKeepsItOut) {
  Compact();
  compact_.OnMouseMoved(kAtTheEdge);
  compact_.OnMouseMoved(kOverThePage);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  compact_.OnMouseMoved(kOverTheSidebar);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, strip_.holds);
}

TEST_F(CompactModeTest, LeavingCompactModeHidesIt) {
  Compact();
  compact_.OnMouseMoved(kAtTheEdge);
  strip_.collapsed = false;
  compact_.OnMouseMoved(kOverTheSidebar);
  EXPECT_EQ(0, strip_.holds);
}

TEST_F(CompactModeTest, ABackgroundTabFlashesItFor800Ms) {
  Compact();
  tabs_.OpenInBackground();
  EXPECT_EQ(1, strip_.holds);
  task_environment_.FastForwardBy(base::Milliseconds(799));
  EXPECT_EQ(1, strip_.holds);
  task_environment_.FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0, strip_.holds);
}

}  // namespace
}  // namespace cromarc
