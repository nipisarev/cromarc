// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_registry.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/browser/page_navigator.h"
#include "cromarc/browser/ports/fake_app_lifecycle.h"
#include "cromarc/browser/ports/fake_glance_surface.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_windows.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

class FakeDelegate : public GlanceRegistry::Delegate {
 public:
  TabKind KindOf(TabId tab) override {
    auto it = kinds.find(tab);
    return it == kinds.end() ? TabKind::kToday : it->second;
  }
  void Archive(TabId glance, bool session_ending) override {
    archived.emplace_back(glance, session_ending);
  }

  std::map<TabId, TabKind> kinds;
  std::vector<std::pair<TabId, bool>> archived;
};

content::OpenURLParams LinkTo(const char* url) {
  return content::OpenURLParams(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
}

class GlanceRegistryTest : public testing::Test {
 protected:
  GlanceRegistryTest() {
    windows_.windows.push_back(window_);
    windows_.windows.push_back(other_window_);
    today_ = tabs_.AddFrom(window_, std::nullopt, GURL("https://a.test/"),
                           /*active=*/true);
    registry_.Watch(window_);
  }

  // A tab of the window showing `url`, active.
  TabId AddTab(const char* url, TabKind kind = TabKind::kToday) {
    const TabId tab = tabs_.AddFrom(window_, std::nullopt, GURL(url),
                                    /*active=*/true);
    tabs_.SetPinned(tab, kind != TabKind::kToday);
    delegate_.kinds[tab] = kind;
    return tab;
  }

  // Option+click on a link of `parent`: its glance.
  TabId Glance(TabId parent, const char* url = "https://b.test/") {
    EXPECT_TRUE(registry_.Expect(parent, gfx::Point(10, 20)));
    tabs_.OpenFrom(parent, LinkTo(url));
    const std::optional<TabId> glance = registry_.GlanceOf(parent);
    EXPECT_TRUE(glance);
    return glance.value_or(TabId(0));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const raw_ptr<BrowserWindowInterface> window_ =
      FakeWindows::Handle<BrowserWindowInterface>(1);
  const raw_ptr<BrowserWindowInterface> other_window_ =
      FakeWindows::Handle<BrowserWindowInterface>(2);
  FakeTabs tabs_;
  FakeWindows windows_;
  FakeAppLifecycle app_lifecycle_;
  FakeGlanceSurface surface_;
  FakeDelegate delegate_;
  GlanceRegistry registry_{tabs_, windows_, app_lifecycle_, surface_,
                           delegate_};
  TabId today_{0};
};

TEST_F(GlanceRegistryTest, OptionClickOpensAGlanceOfItsTab) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  EXPECT_TRUE(registry_.IsGlance(glance));
  EXPECT_EQ(registry_.ParentOf(glance), parent);
  EXPECT_EQ(tabs_.GetActive(window_), glance);
  EXPECT_TRUE(surface_.snapshots.contains(parent));
  ASSERT_TRUE(surface_.shown.contains(window_));
  EXPECT_EQ(surface_.shown[window_].glance, glance);
  EXPECT_EQ(surface_.shown[window_].parent, parent);
  EXPECT_EQ(surface_.shown[window_].origin, gfx::Point(10, 20));
}

TEST_F(GlanceRegistryTest, AGlanceOpensNoGlanceAndATabHasOne) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  EXPECT_FALSE(registry_.Expect(glance, std::nullopt));
  EXPECT_FALSE(registry_.Expect(parent, std::nullopt));
}

TEST_F(GlanceRegistryTest, AwayAndBackTheGlanceHidesAndShowsWithItsTab) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  tabs_.Activate(today_);
  EXPECT_FALSE(surface_.shown.contains(window_));
  EXPECT_TRUE(registry_.IsGlance(glance));

  tabs_.Activate(parent);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(tabs_.GetActive(window_), glance);
  ASSERT_TRUE(surface_.shown.contains(window_));
  // Shown again at once, not grown again.
  EXPECT_EQ(surface_.shown[window_].origin, std::nullopt);
}

TEST_F(GlanceRegistryTest, AClickOnItsTabGoesStraightToTheGlance) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  tabs_.Activate(today_);
  registry_.ActivateTabOrGlance(parent);
  EXPECT_EQ(tabs_.GetActive(window_), glance);
  registry_.ActivateTabOrGlance(today_);
  EXPECT_EQ(tabs_.GetActive(window_), today_);
}

TEST_F(GlanceRegistryTest, TheUserClosesItAfterItShrinksAndNothingIsArchived) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.Close(glance);
  EXPECT_TRUE(tabs_.Exists(glance));
  surface_.FinishHide(window_);
  EXPECT_FALSE(tabs_.Exists(glance));
  EXPECT_EQ(tabs_.GetActive(window_), parent);
  EXPECT_FALSE(registry_.GlanceOf(parent));
  EXPECT_TRUE(delegate_.archived.empty());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(tabs_.GetActive(window_), parent);
}

TEST_F(GlanceRegistryTest, CloseAndExpandActiveActOnlyOnAGlance) {
  EXPECT_FALSE(registry_.CloseActive(window_));
  EXPECT_FALSE(registry_.ExpandActive(window_));
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  EXPECT_TRUE(registry_.CloseActive(window_));
  surface_.FinishHide(window_);
  EXPECT_FALSE(tabs_.Exists(glance));
}

TEST_F(GlanceRegistryTest, EscClosesIt) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.EscapeGlance(window_, /*in_field=*/false);
  surface_.FinishHide(window_);
  EXPECT_FALSE(tabs_.Exists(glance));
  EXPECT_EQ(tabs_.GetActive(window_), parent);
  EXPECT_TRUE(surface_.confirm_asks.empty());
}

TEST_F(GlanceRegistryTest, EscInAFieldAsksFirstAndEscAgainClosesIt) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.EscapeGlance(window_, /*in_field=*/true);
  surface_.FinishHide(window_);
  EXPECT_TRUE(tabs_.Exists(glance));
  EXPECT_EQ(surface_.confirm_asks,
            std::vector<BrowserWindowInterface*>{window_});

  task_environment_.FastForwardBy(base::Seconds(2));
  registry_.EscapeGlance(window_, /*in_field=*/true);
  surface_.FinishHide(window_);
  EXPECT_FALSE(tabs_.Exists(glance));
  EXPECT_EQ(surface_.confirm_asks.size(), 1u);
}

TEST_F(GlanceRegistryTest, OnceTheCloseButtonStopsAskingEscInAFieldAsksAgain) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.EscapeGlance(window_, /*in_field=*/true);
  task_environment_.FastForwardBy(base::Seconds(3));
  registry_.EscapeGlance(window_, /*in_field=*/true);
  surface_.FinishHide(window_);
  EXPECT_TRUE(tabs_.Exists(glance));
  EXPECT_EQ(surface_.confirm_asks.size(), 2u);
}

TEST_F(GlanceRegistryTest, ShownAgainEscInAFieldAsksAgain) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.EscapeGlance(window_, /*in_field=*/true);
  tabs_.Activate(today_);
  registry_.ActivateTabOrGlance(parent);
  ASSERT_EQ(tabs_.GetActive(window_), glance);
  registry_.EscapeGlance(window_, /*in_field=*/true);
  surface_.FinishHide(window_);
  EXPECT_TRUE(tabs_.Exists(glance));
  EXPECT_EQ(surface_.confirm_asks.size(), 2u);
}

TEST_F(GlanceRegistryTest, EscWithoutAGlanceDoesNothing) {
  const TabId tab = AddTab("https://a.test/page");
  registry_.EscapeGlance(window_, /*in_field=*/false);
  EXPECT_TRUE(tabs_.Exists(tab));
  EXPECT_EQ(tabs_.GetActive(window_), tab);
}

TEST_F(GlanceRegistryTest, ClosingItsTabArchivesAndClosesTheGlance) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  tabs_.Remove(parent);
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_EQ(delegate_.archived[0].first, glance);
  EXPECT_FALSE(registry_.IsGlance(glance));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(tabs_.Exists(glance));
}

TEST_F(GlanceRegistryTest, AGlanceClosedByItsWindowIsArchived) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  tabs_.Remove(glance);
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_EQ(delegate_.archived[0].first, glance);
  EXPECT_FALSE(registry_.GlanceOf(parent));
}

TEST_F(GlanceRegistryTest, ItsTabMovingToAnotherWindowArchivesTheGlance) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.Watch(other_window_);
  tabs_.MoveToWindow(parent, other_window_);
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_EQ(delegate_.archived[0].first, glance);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(tabs_.Exists(glance));
}

TEST_F(GlanceRegistryTest, ExpandedFromAnEssentialItIsTheFirstTodayTab) {
  const TabId parent = AddTab("https://a.test/page", TabKind::kEssential);
  const TabId glance = Glance(parent);
  registry_.Expand(glance);
  EXPECT_FALSE(registry_.IsGlance(glance));
  EXPECT_FALSE(surface_.shown.contains(window_));
  EXPECT_EQ(tabs_.GetActive(window_), glance);
  const std::vector<TabId> list = tabs_.List(window_);
  EXPECT_EQ(list, (std::vector<TabId>{parent, glance, today_}));
}

TEST_F(GlanceRegistryTest, ExpandedFromATodayTabItStaysRightAfterIt) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  registry_.Expand(glance);
  EXPECT_EQ(tabs_.List(window_), (std::vector<TabId>{today_, parent, glance}));
  EXPECT_TRUE(delegate_.archived.empty());
}

TEST_F(GlanceRegistryTest, AQuitArchivesAndClosesEveryGlance) {
  const TabId first = Glance(AddTab("https://a.test/1"));
  const TabId second = Glance(AddTab("https://a.test/2", TabKind::kPinned));
  app_lifecycle_.CloseAllWindows(true);
  ASSERT_EQ(delegate_.archived.size(), 2u);
  EXPECT_TRUE(delegate_.archived[0].second);
  EXPECT_FALSE(tabs_.Exists(first));
  EXPECT_FALSE(tabs_.Exists(second));
  // Archived once: the tabs closing are no glances any more.
  EXPECT_EQ(delegate_.archived.size(), 2u);
}

TEST_F(GlanceRegistryTest, AnotherSiteAnEssentialOpensInATabIsItsGlance) {
  const TabId essential =
      AddTab("https://github.com/org/pull/1", TabKind::kEssential);
  const TabId opened = tabs_.AddFrom(window_, essential,
                                     GURL("https://linear.app/x"), true);
  EXPECT_EQ(registry_.ParentOf(opened), essential);
  EXPECT_TRUE(surface_.snapshots.contains(essential));
  EXPECT_TRUE(surface_.shown.contains(window_));
}

TEST_F(GlanceRegistryTest, WhatAnEssentialOpensOtherwiseIsATab) {
  const TabId essential =
      AddTab("https://github.com/org/pull/1", TabKind::kEssential);
  // In the background (Cmd+click): the user's choice of a tab.
  const TabId background = tabs_.AddFrom(
      window_, essential, GURL("https://linear.app/x"), /*active=*/false);
  EXPECT_FALSE(registry_.IsGlance(background));
  // The same site.
  const TabId same = tabs_.AddFrom(window_, essential,
                                   GURL("https://github.com/org/pull/2"), true);
  EXPECT_FALSE(registry_.IsGlance(same));
}

TEST_F(GlanceRegistryTest, WhatATodayTabOrAGlanceOpensIsATab) {
  const TabId today = AddTab("https://github.com/org/pull/1");
  const TabId from_today =
      tabs_.AddFrom(window_, today, GURL("https://linear.app/x"), true);
  EXPECT_FALSE(registry_.IsGlance(from_today));

  const TabId essential =
      AddTab("https://github.com/org/pull/1", TabKind::kEssential);
  const TabId glance = Glance(essential, "https://linear.app/x");
  const TabId from_glance =
      tabs_.AddFrom(window_, glance, GURL("https://notion.so/y"), true);
  EXPECT_FALSE(registry_.IsGlance(from_glance));
}

TEST_F(GlanceRegistryTest, ATabTheUserAskedForIsATab) {
  const TabId essential =
      AddTab("https://github.com/org/pull/1", TabKind::kEssential);
  registry_.ExpectTab(essential);
  const TabId opened =
      tabs_.AddFrom(window_, essential, GURL("https://linear.app/x"), true);
  EXPECT_FALSE(registry_.IsGlance(opened));
  // Only that one.
  const TabId next =
      tabs_.AddFrom(window_, essential, GURL("https://linear.app/y"), true);
  EXPECT_TRUE(registry_.IsGlance(next));
}

TEST_F(GlanceRegistryTest, WindowOpenIsDecidedWhenItsAddressIsKnown) {
  const TabId essential =
      AddTab("https://github.com/org/pull/1", TabKind::kEssential);
  const TabId opened = tabs_.AddFrom(window_, essential, GURL(), true);
  EXPECT_FALSE(registry_.IsGlance(opened));
  registry_.OnNavigationStarting(opened, GURL("https://linear.app/x"));
  EXPECT_EQ(registry_.ParentOf(opened), essential);
  // Only the first navigation decides.
  const TabId later = tabs_.AddFrom(window_, today_, GURL(), true);
  registry_.OnNavigationStarting(later, GURL("https://linear.app/y"));
  EXPECT_FALSE(registry_.IsGlance(later));
}

TEST_F(GlanceRegistryTest, AnExpectationTooOldOpensAnOrdinaryTab) {
  const TabId parent = AddTab("https://a.test/page");
  ASSERT_TRUE(registry_.Expect(parent, std::nullopt));
  task_environment_.FastForwardBy(base::Seconds(11));
  tabs_.OpenFrom(parent, LinkTo("https://b.test/"));
  EXPECT_FALSE(registry_.GlanceOf(parent));
}

TEST_F(GlanceRegistryTest, GlancesInAWindow) {
  const TabId parent = AddTab("https://a.test/page");
  const TabId glance = Glance(parent);
  const std::vector<GlanceRegistry::Entry> entries =
      registry_.GlancesIn(window_);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].glance, glance);
  EXPECT_EQ(entries[0].parent, parent);
  EXPECT_TRUE(registry_.GlancesIn(other_window_).empty());
}

}  // namespace
}  // namespace cromarc
