// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_rules.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

// A user's click on an Essential's page, to another site.
LinkNavigation ClickToAnotherSite() {
  LinkNavigation navigation;
  navigation.in_primary_main_frame = true;
  navigation.renderer_initiated = true;
  navigation.user_gesture = true;
  navigation.tab_kind = TabKind::kEssential;
  navigation.page = GURL("https://github.com/org/repo/pull/1");
  navigation.url = GURL("https://linear.app/issue/1");
  return navigation;
}

TEST(GlanceRulesTest, AClickFromAnEssentialToAnotherSiteOpensInGlance) {
  EXPECT_TRUE(LeavesForGlance(ClickToAnotherSite()));
}

TEST(GlanceRulesTest, AClickFromAPinnedTabToAnotherSiteOpensInGlance) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.tab_kind = TabKind::kPinned;
  EXPECT_TRUE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, ATodayTabFollowsItsLinks) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.tab_kind = TabKind::kToday;
  EXPECT_FALSE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, AGlanceFollowsItsLinks) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.tab_is_glance = true;
  EXPECT_FALSE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, ALinkToTheSameSiteStays) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.url = GURL("https://github.com/org/repo/pull/2");
  EXPECT_FALSE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, AWwwInFrontIsTheSameSite) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.url = GURL("https://www.github.com/org");
  EXPECT_FALSE(LeavesForGlance(navigation));
  navigation.page = GURL("https://www.example.com/");
  navigation.url = GURL("https://example.com/a");
  EXPECT_FALSE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, ASubdomainIsAnotherSite) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.url = GURL("https://gist.github.com/x");
  EXPECT_TRUE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, OnlyAUsersLinkInTheTopPageOpensInGlance) {
  LinkNavigation subframe = ClickToAnotherSite();
  subframe.in_primary_main_frame = false;
  EXPECT_FALSE(LeavesForGlance(subframe));
  LinkNavigation no_gesture = ClickToAnotherSite();
  no_gesture.user_gesture = false;
  EXPECT_FALSE(LeavesForGlance(no_gesture));
  LinkNavigation typed = ClickToAnotherSite();
  typed.renderer_initiated = false;
  EXPECT_FALSE(LeavesForGlance(typed));
}

TEST(GlanceRulesTest, ReloadsHistoryAndInPageNavigationsStay) {
  LinkNavigation reload = ClickToAnotherSite();
  reload.reload_or_history = true;
  EXPECT_FALSE(LeavesForGlance(reload));
  LinkNavigation fragment = ClickToAnotherSite();
  fragment.same_document = true;
  EXPECT_FALSE(LeavesForGlance(fragment));
}

TEST(GlanceRulesTest, AFormPostStaysInTheTab) {
  LinkNavigation navigation = ClickToAnotherSite();
  navigation.post = true;
  EXPECT_FALSE(LeavesForGlance(navigation));
}

TEST(GlanceRulesTest, OnlyWebAndFilePagesOpenInGlance) {
  for (const char* url : {"mailto:a@b.c", "javascript:void(0)",
                          "data:text/html,x", "chrome://settings/"}) {
    LinkNavigation navigation = ClickToAnotherSite();
    navigation.url = GURL(url);
    EXPECT_FALSE(LeavesForGlance(navigation)) << url;
  }
}

OpenedTab OpenedFromAnEssential() {
  OpenedTab tab;
  tab.opener_kind = TabKind::kEssential;
  tab.opener_page = GURL("https://github.com/org/repo/pull/1");
  tab.url = GURL("https://linear.app/issue/1");
  return tab;
}

TEST(GlanceRulesTest, AnotherSiteAnEssentialOpensInATabIsItsGlance) {
  EXPECT_TRUE(OpensAsGlance(OpenedFromAnEssential()));
  OpenedTab pinned = OpenedFromAnEssential();
  pinned.opener_kind = TabKind::kPinned;
  EXPECT_TRUE(OpensAsGlance(pinned));
}

TEST(GlanceRulesTest, ATabOfTheSameSiteIsATab) {
  OpenedTab tab = OpenedFromAnEssential();
  tab.url = GURL("https://github.com/org/repo/pull/2");
  EXPECT_FALSE(OpensAsGlance(tab));
}

TEST(GlanceRulesTest, ATabOpenedHasNoWwwExemption) {
  // Zen compares hosts exactly for a tab a page opens.
  OpenedTab tab = OpenedFromAnEssential();
  tab.url = GURL("https://www.github.com/org");
  EXPECT_TRUE(OpensAsGlance(tab));
}

TEST(GlanceRulesTest, ATabATodayTabOrAGlanceOpensIsATab) {
  OpenedTab from_today = OpenedFromAnEssential();
  from_today.opener_kind = TabKind::kToday;
  EXPECT_FALSE(OpensAsGlance(from_today));
  OpenedTab from_glance = OpenedFromAnEssential();
  from_glance.opener_is_glance = true;
  EXPECT_FALSE(OpensAsGlance(from_glance));
}

TEST(GlanceRulesTest, ATabOfNoWebPageIsATab) {
  OpenedTab tab = OpenedFromAnEssential();
  tab.url = GURL("about:blank");
  EXPECT_FALSE(OpensAsGlance(tab));
}

}  // namespace
}  // namespace cromarc
