// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/policy.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

class PolicyTest : public testing::Test {
 protected:
  TabFacts Today(base::TimeDelta idle) {
    TabFacts facts;
    facts.last_active = now_ - idle;
    return facts;
  }

  const base::Time now_ = base::Time::Now();
};

TEST_F(PolicyTest, TodayTabsGoToTheArchiveAfterTwelveHours) {
  EXPECT_EQ(Decide(Today(base::Hours(11)), now_), Decision::kKeep);
  EXPECT_EQ(Decide(Today(base::Hours(12) + base::Minutes(1)), now_),
            Decision::kArchive);
}

TEST_F(PolicyTest, PinnedTabsFallAsleepAfterTwelveHours) {
  TabFacts facts = Today(base::Hours(13));
  facts.pinned_item = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kSleep);
}

TEST_F(PolicyTest, TheActiveTabOfAVisibleWindowNeverIdles) {
  TabFacts facts = Today(base::Hours(30));
  facts.active_in_visible_window = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kKeep);
}

TEST_F(PolicyTest, WhenTheUserIsAwayTheActiveTabIdlesFromTheLastInput) {
  TabFacts facts = Today(base::Hours(30));
  facts.active_in_visible_window = true;
  facts.user_away_for = base::Hours(12) + base::Minutes(5);
  EXPECT_EQ(Decide(facts, now_), Decision::kArchive);
  facts.user_away_for = base::Hours(2);
  EXPECT_EQ(Decide(facts, now_), Decision::kKeep);
}

TEST_F(PolicyTest, SoundAndCapturePauseTheClock) {
  TabFacts facts = Today(base::Hours(20));
  facts.paused = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kKeep);
}

TEST_F(PolicyTest, UnsavedInputDefersInsteadOfClosing) {
  TabFacts facts = Today(base::Hours(20));
  facts.unsaved_input = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kDefer);
  facts.pinned_item = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kDefer);
}

TEST_F(PolicyTest, KeptDeferredTabsStayUntilTheUserActs) {
  TabFacts facts = Today(base::Hours(20));
  facts.unsaved_input = true;
  facts.kept_by_user = true;
  EXPECT_EQ(Decide(facts, now_), Decision::kKeep);
}

TEST_F(PolicyTest, RemainingTimeIsShownInTheLastHourOnly) {
  EXPECT_EQ(TimeToArchive(Today(base::Hours(11) + base::Minutes(20)), now_),
            base::Minutes(40));
  EXPECT_EQ(TimeToArchive(Today(base::Hours(2)), now_), base::TimeDelta());
  TabFacts active = Today(base::Hours(11) + base::Minutes(30));
  active.active_in_visible_window = true;
  EXPECT_EQ(TimeToArchive(active, now_), base::TimeDelta());
}

// Little Arc windows archive every 6 h by default, half the tab limit.
TEST_F(PolicyTest, ALittleWindowUnusedForSixHoursGoes) {
  EXPECT_FALSE(LittleIdledOut(now_ - base::Hours(5) - base::Minutes(59), now_));
  EXPECT_TRUE(LittleIdledOut(now_ - base::Hours(6), now_));
}

TEST_F(PolicyTest, TheIdleSwitchGivesLittleWindowsHalf) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII("cromarc-idle-limit",
                                                          "8");
  const Limits limits = LimitsFromCommandLine();
  EXPECT_EQ(limits.idle, base::Seconds(8));
  EXPECT_EQ(limits.little_idle, base::Seconds(4));
  EXPECT_TRUE(LittleIdledOut(now_ - base::Seconds(4), now_, limits));
}

TEST_F(PolicyTest, WithoutTheSwitchTheLimitsAreArcs) {
  const Limits limits = LimitsFromCommandLine();
  EXPECT_EQ(limits.idle, base::Hours(12));
  EXPECT_EQ(limits.little_idle, base::Hours(6));
}

}  // namespace
}  // namespace cromarc
