// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/shared_sidebar_state.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "cromarc/browser/ports/fake_vertical_strip.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

class SharedSidebarStateTest : public testing::Test {
 protected:
  SharedSidebarState shared_{base::BindRepeating(
      [](int* changes) { ++*changes; }, base::Unretained(&changes_))};
  int changes_ = 0;
};

TEST_F(SharedSidebarStateTest, TheFirstStripGivesTheState) {
  FakeVerticalStrip first;
  first.collapsed = true;
  first.expanded_width = 300;
  base::ScopedClosureRunner added = shared_.Add(first);
  ASSERT_TRUE(shared_.state());
  EXPECT_TRUE(shared_.state()->collapsed);
  EXPECT_EQ(300, shared_.state()->width);
  EXPECT_EQ(1, changes_);
  EXPECT_TRUE(first.collapse_requests.empty());
}

TEST_F(SharedSidebarStateTest, ALaterStripTakesIt) {
  FakeVerticalStrip first;
  first.collapsed = true;
  first.expanded_width = 300;
  base::ScopedClosureRunner added_first = shared_.Add(first);
  FakeVerticalStrip second;
  base::ScopedClosureRunner added_second = shared_.Add(second);
  EXPECT_TRUE(second.collapsed);
  EXPECT_EQ(300, second.expanded_width);
  // Taking the state is no change of it.
  EXPECT_EQ(1, changes_);
}

TEST_F(SharedSidebarStateTest, ACollapseGoesToTheOthers) {
  FakeVerticalStrip first;
  FakeVerticalStrip second;
  FakeVerticalStrip third;
  base::ScopedClosureRunner a = shared_.Add(first);
  base::ScopedClosureRunner b = shared_.Add(second);
  base::ScopedClosureRunner c = shared_.Add(third);
  second.UserCollapses(true);
  EXPECT_TRUE(first.collapsed);
  EXPECT_TRUE(third.collapsed);
  // Not back to the strip it came from.
  EXPECT_TRUE(second.collapse_requests.empty());
  EXPECT_EQ(std::vector<bool>{true}, first.collapse_requests);
  EXPECT_EQ(2, changes_);
}

TEST_F(SharedSidebarStateTest, ACollapsingStripCountsAsCollapsed) {
  FakeVerticalStrip first;
  FakeVerticalStrip second;
  base::ScopedClosureRunner a = shared_.Add(first);
  base::ScopedClosureRunner b = shared_.Add(second);
  first.collapsing = true;
  first.UserCollapses(false);
  EXPECT_TRUE(second.collapsed);
}

TEST_F(SharedSidebarStateTest, AWidthIsSharedOnceTheDragEnds) {
  FakeVerticalStrip first;
  FakeVerticalStrip second;
  base::ScopedClosureRunner a = shared_.Add(first);
  base::ScopedClosureRunner b = shared_.Add(second);
  first.resizing = true;
  first.expanded_width = 280;
  first.UserCollapses(false);
  EXPECT_EQ(240, second.expanded_width);
  first.UserResizes(320);
  EXPECT_EQ(320, second.expanded_width);
  EXPECT_EQ(320, shared_.state()->width);
}

TEST_F(SharedSidebarStateTest, ARemovedStripTakesNothing) {
  FakeVerticalStrip first;
  FakeVerticalStrip second;
  base::ScopedClosureRunner a = shared_.Add(first);
  {
    base::ScopedClosureRunner b = shared_.Add(second);
  }
  first.UserCollapses(true);
  EXPECT_FALSE(second.collapsed);
  // Nor does it give anything.
  second.UserCollapses(false);
  EXPECT_TRUE(first.collapsed);
}

TEST_F(SharedSidebarStateTest, NoChangeIsNoChange) {
  FakeVerticalStrip first;
  base::ScopedClosureRunner a = shared_.Add(first);
  first.UserCollapses(false);
  EXPECT_EQ(1, changes_);
}

}  // namespace
}  // namespace cromarc
