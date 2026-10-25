// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/swipe_tracker.h"

#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

using Phase = SwipeTracker::Phase;

class SwipeTrackerTest : public testing::Test {
 protected:
  std::vector<SwipeTracker::Event> Feed(Phase phase, double dx, double dy,
                                        bool momentum = false) {
    now_ += base::Milliseconds(16);
    return tracker_.Process({phase, momentum, dx, dy, now_});
  }

  SwipeTracker tracker_;
  base::TimeTicks now_ = base::TimeTicks::Now();
};

TEST_F(SwipeTrackerTest, AVerticalScrollIsNotASwipe) {
  Feed(Phase::kBegan, 0, 0);
  EXPECT_TRUE(Feed(Phase::kChanged, 1, -12).empty());
  EXPECT_FALSE(tracker_.consuming());
  EXPECT_TRUE(Feed(Phase::kChanged, 0, -8).empty());
  EXPECT_TRUE(Feed(Phase::kEnded, 0, 0).empty());
}

TEST_F(SwipeTrackerTest, AHorizontalScrollStartsAndReportsTheCumulativeAmount) {
  Feed(Phase::kBegan, 0, 0);
  std::vector<SwipeTracker::Event> events = Feed(Phase::kChanged, -55, 2);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].type, SwipeTracker::EventType::kStart);
  EXPECT_EQ(events[1].type, SwipeTracker::EventType::kUpdate);
  // Fingers moving left move content left: towards the next space.
  EXPECT_NEAR(events[1].amount, 55.0 / SwipeTracker::kPixelSize, 1e-9);
  EXPECT_TRUE(tracker_.consuming());
  events = Feed(Phase::kChanged, -55, 0);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_NEAR(events[0].amount, 110.0 / SwipeTracker::kPixelSize, 1e-9);
}

TEST_F(SwipeTrackerTest, ReleasingPastTheThresholdSucceeds) {
  Feed(Phase::kBegan, 0, 0);
  for (int i = 0; i < 10; ++i) {
    Feed(Phase::kChanged, -20, 0);
  }
  // A pause before lifting the fingers: no velocity, position decides.
  now_ += base::Milliseconds(300);
  Feed(Phase::kChanged, 0, 0);
  std::vector<SwipeTracker::Event> events = Feed(Phase::kEnded, 0, 0);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].type, SwipeTracker::EventType::kEnd);
  EXPECT_EQ(events[0].direction, 1);  // next space
}

TEST_F(SwipeTrackerTest, AShortSlowSwipeIsCancelled) {
  Feed(Phase::kBegan, 0, 0);
  Feed(Phase::kChanged, 30, 0);
  now_ += base::Milliseconds(300);
  Feed(Phase::kChanged, 0, 0);
  std::vector<SwipeTracker::Event> events = Feed(Phase::kEnded, 0, 0);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].direction, 0);
}

TEST_F(SwipeTrackerTest, AFlickCountsVelocity) {
  Feed(Phase::kBegan, 0, 0);
  Feed(Phase::kChanged, 40, 0);
  Feed(Phase::kChanged, 40, 0);
  std::vector<SwipeTracker::Event> events = Feed(Phase::kEnded, 0, 0);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].direction, -1);  // previous space
}

TEST_F(SwipeTrackerTest, MomentumAfterTheSwipeIsSwallowed) {
  Feed(Phase::kBegan, 0, 0);
  Feed(Phase::kChanged, -80, 0);
  Feed(Phase::kEnded, 0, 0);
  EXPECT_TRUE(tracker_.consuming());
  EXPECT_TRUE(Feed(Phase::kChanged, -30, 0, /*momentum=*/true).empty());
  EXPECT_TRUE(tracker_.consuming());
  Feed(Phase::kEnded, 0, 0, /*momentum=*/true);
  EXPECT_FALSE(tracker_.consuming());
}

// The swipe brought another window up, which got the momentum, or there was
// none: the mouse wheel must scroll again.
TEST_F(SwipeTrackerTest, AWheelEndsTheWaitForMomentumThatWentElsewhere) {
  Feed(Phase::kBegan, 0, 0);
  Feed(Phase::kChanged, -80, 0);
  Feed(Phase::kEnded, 0, 0);
  ASSERT_TRUE(tracker_.consuming());
  EXPECT_TRUE(Feed(Phase::kNone, 0, 10).empty());
  EXPECT_FALSE(tracker_.consuming());
}

TEST_F(SwipeTrackerTest, AWheelCancelsASwipeUnderWay) {
  Feed(Phase::kBegan, 0, 0);
  Feed(Phase::kChanged, -80, 0);
  std::vector<SwipeTracker::Event> events = Feed(Phase::kNone, 0, 10);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].type, SwipeTracker::EventType::kEnd);
  EXPECT_EQ(events[0].direction, 0);
  EXPECT_FALSE(tracker_.consuming());
}

}  // namespace
}  // namespace cromarc
