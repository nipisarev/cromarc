// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_menu.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

constexpr int kBack = 1;
constexpr int kNewTab = 10;
constexpr int kNewWindow = 11;
constexpr int kIncognito = 12;
constexpr int kSaveLink = 20;
constexpr int kCopyLink = 21;

MenuContext LinkInATab() {
  MenuContext context;
  context.link = GURL("https://linear.app/issue/1");
  context.in_normal_tab = true;
  return context;
}

TEST(GlanceMenuTest, AWebLinkInATabOffersOpenLinkInGlance) {
  EXPECT_TRUE(OffersOpenLinkInGlance(LinkInATab()));
}

TEST(GlanceMenuTest, NoLinkOrNoWebLinkOffersNothing) {
  MenuContext no_link = LinkInATab();
  no_link.link = GURL();
  EXPECT_FALSE(OffersOpenLinkInGlance(no_link));
  MenuContext mail = LinkInATab();
  mail.link = GURL("mailto:a@b.c");
  EXPECT_FALSE(OffersOpenLinkInGlance(mail));
}

TEST(GlanceMenuTest, AGlanceALittleWindowOrIncognitoOffersNothing) {
  MenuContext in_glance = LinkInATab();
  in_glance.in_glance = true;
  EXPECT_FALSE(OffersOpenLinkInGlance(in_glance));
  EXPECT_FALSE(SearchOpensInGlance(in_glance));
  // A Little window, an app or an incognito window: no normal tab.
  MenuContext elsewhere = LinkInATab();
  elsewhere.in_normal_tab = false;
  EXPECT_FALSE(OffersOpenLinkInGlance(elsewhere));
  EXPECT_FALSE(SearchOpensInGlance(elsewhere));
}

TEST(GlanceMenuTest, ItGoesLastAmongTheOpenLinkItems) {
  const std::vector<int> menu = {kNewTab, kNewWindow, kIncognito,
                                 kMenuSeparator, kSaveLink, kCopyLink};
  EXPECT_EQ(OpenLinkInGlanceIndex(menu, kNewTab), 3u);
}

TEST(GlanceMenuTest, ItFindsTheOpenLinkItemsAfterOthers) {
  const std::vector<int> menu = {kBack, kMenuSeparator, kNewTab, kNewWindow,
                                 kMenuSeparator, kSaveLink};
  EXPECT_EQ(OpenLinkInGlanceIndex(menu, kNewTab), 4u);
}

TEST(GlanceMenuTest, WithNoSeparatorAfterThemItGoesLast) {
  const std::vector<int> menu = {kNewTab, kNewWindow};
  EXPECT_EQ(OpenLinkInGlanceIndex(menu, kNewTab), 2u);
}

TEST(GlanceMenuTest, WithoutOpenLinkItemsThereIsNoPlace) {
  const std::vector<int> menu = {kBack, kMenuSeparator, kCopyLink};
  EXPECT_EQ(OpenLinkInGlanceIndex(menu, kNewTab), std::nullopt);
}

TEST(GlanceMenuTest, SearchFromATabOpensInGlance) {
  MenuContext selection = LinkInATab();
  selection.link = GURL();
  EXPECT_TRUE(SearchOpensInGlance(selection));
}

}  // namespace
}  // namespace cromarc
