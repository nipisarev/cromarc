// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/arc_import/arc_sidebar.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

// The shape of Arc's StorableSidebar.json (2026), trimmed: two spaces bound
// to Arc profiles, a pinned tree with a folder of a folder, a split view,
// essentials per profile.
constexpr char kSidebar[] = R"json({
  "version": 1,
  "sidebar": {"containers": [
    {"global": {}},
    {
      "spaces": [
        "s-personal",
        {"id": "s-personal", "title": "Personal",
         "customInfo": {"iconType": {"emoji_v2": "🏠"},
                        "windowTheme": {"primaryColorPalette": {"midTone":
                            {"red": 1.0, "green": 0.5, "blue": 0.0,
                             "alpha": 1, "colorSpace": "extendedSRGB"}}}},
         "profile": {"default": true},
         "containerIDs": ["unpinned", "c-u1", "pinned", "c-p1"]},
        "s-work",
        {"id": "s-work", "title": "Work",
         "customInfo": {"iconType": {"icon": "briefcase"}},
         "profile": {"custom": {"_0": {"machineID": "m-1",
                                        "directoryBasename": "Profile 2"}}},
         "containerIDs": ["pinned", "c-p2", "unpinned", "c-u2"]}
      ],
      "items": [
        "c-p1", {"id": "c-p1", "childrenIds": ["git", "t-mail"],
                 "data": {"itemContainer": {"containerType": {"spaceItems": {}}}}},
        "git", {"id": "git", "title": "Git", "childrenIds": ["t-app", "qase"],
                "data": {"list": {}}},
        "t-app", {"id": "t-app", "title": "Tiden APP", "childrenIds": [],
                  "data": {"tab": {"savedURL": "https://github.com/t/app/pulls",
                                   "savedTitle": "Pull requests · t/app"}}},
        "qase", {"id": "qase", "title": "Qase", "childrenIds": ["t-stage"],
                 "data": {"list": {}}},
        "t-stage", {"id": "t-stage", "title": null, "childrenIds": [],
                    "data": {"tab": {"savedURL": "https://stage.qase.io",
                                     "savedTitle": "Stage"}}},
        "t-mail", {"id": "t-mail", "title": null, "childrenIds": [],
                   "data": {"tab": {"savedURL": "https://mail.example",
                                    "savedTitle": "Mail"}}},
        "c-u1", {"id": "c-u1", "childrenIds": ["t-today"],
                 "data": {"itemContainer": {"containerType": {"spaceItems": {}}}}},
        "t-today", {"id": "t-today", "title": null, "childrenIds": [],
                    "data": {"tab": {"savedURL": "https://today.example",
                                     "savedTitle": "Today"}}},
        "c-p2", {"id": "c-p2", "childrenIds": ["split"],
                 "data": {"itemContainer": {"containerType": {"spaceItems": {}}}}},
        "split", {"id": "split", "title": null, "childrenIds": ["t-left", "t-right"],
                  "data": {"splitView": {}}},
        "t-left", {"id": "t-left", "title": null, "childrenIds": [],
                   "data": {"tab": {"savedURL": "https://left.example",
                                    "savedTitle": "Left"}}},
        "t-right", {"id": "t-right", "title": null, "childrenIds": [],
                    "data": {"tab": {"savedURL": "https://right.example",
                                     "savedTitle": "Right"}}},
        "c-u2", {"id": "c-u2", "childrenIds": [],
                 "data": {"itemContainer": {"containerType": {"spaceItems": {}}}}},
        "top-default", {"id": "top-default", "childrenIds": ["t-cal"],
                        "data": {"itemContainer": {"containerType": {"topApps": {}}}}},
        "t-cal", {"id": "t-cal", "title": null, "childrenIds": [],
                  "data": {"tab": {"savedURL": "https://calendar.example",
                                   "savedTitle": "Calendar"}}},
        "top-work", {"id": "top-work", "childrenIds": ["t-slack"],
                     "data": {"itemContainer": {"containerType": {"topApps": {}}}}},
        "t-slack", {"id": "t-slack", "title": null, "childrenIds": [],
                    "data": {"tab": {"savedURL": "https://slack.example",
                                     "savedTitle": "Slack"}}}
      ],
      "topAppsContainerIDs": [
        {"default": true}, "top-default",
        {"custom": {"_0": {"machineID": "m-1", "directoryBasename": "Profile 2"}}},
        "top-work"
      ]
    }
  ]}
})json";

TEST(ArcSidebarTest, SpacesKeepTheirOrderTitleIconAndArcProfile) {
  std::optional<ArcSidebar> sidebar = ParseArcSidebar(kSidebar);
  ASSERT_TRUE(sidebar);
  ASSERT_EQ(sidebar->spaces.size(), 2u);
  EXPECT_EQ(sidebar->spaces[0].title, "Personal");
  EXPECT_EQ(sidebar->spaces[0].icon, "🏠");
  EXPECT_EQ(sidebar->spaces[0].arc_profile, "");
  EXPECT_EQ(sidebar->spaces[0].color, SkColorSetRGB(255, 128, 0));
  EXPECT_EQ(sidebar->spaces[1].title, "Work");
  // Arc's own icons are SF Symbols, not emoji.
  EXPECT_EQ(sidebar->spaces[1].icon, "");
  EXPECT_FALSE(sidebar->spaces[1].color);
  EXPECT_EQ(sidebar->spaces[1].arc_profile, "Profile 2");
}

TEST(ArcSidebarTest, PinnedTreeNestsFoldersAndPrefersTheUsersTitle) {
  std::optional<ArcSidebar> sidebar = ParseArcSidebar(kSidebar);
  ASSERT_TRUE(sidebar);
  const std::vector<ArcNode>& pinned = sidebar->spaces[0].pinned;
  ASSERT_EQ(pinned.size(), 2u);
  EXPECT_TRUE(pinned[0].is_folder);
  EXPECT_EQ(pinned[0].title, u"Git");
  ASSERT_EQ(pinned[0].children.size(), 2u);
  // A renamed tab keeps the user's name, not the page title.
  EXPECT_EQ(pinned[0].children[0].title, u"Tiden APP");
  EXPECT_EQ(pinned[0].children[0].url, GURL("https://github.com/t/app/pulls"));
  EXPECT_TRUE(pinned[0].children[1].is_folder);
  ASSERT_EQ(pinned[0].children[1].children.size(), 1u);
  EXPECT_EQ(pinned[0].children[1].children[0].title, u"Stage");
  EXPECT_EQ(pinned[1].title, u"Mail");
}

TEST(ArcSidebarTest, SplitViewsBecomeTheirTabs) {
  std::optional<ArcSidebar> sidebar = ParseArcSidebar(kSidebar);
  ASSERT_TRUE(sidebar);
  const std::vector<ArcNode>& pinned = sidebar->spaces[1].pinned;
  ASSERT_EQ(pinned.size(), 2u);
  EXPECT_EQ(pinned[0].title, u"Left");
  EXPECT_EQ(pinned[1].title, u"Right");
}

TEST(ArcSidebarTest, EssentialsBelongToArcProfiles) {
  std::optional<ArcSidebar> sidebar = ParseArcSidebar(kSidebar);
  ASSERT_TRUE(sidebar);
  ASSERT_EQ(sidebar->essentials[""].size(), 1u);
  EXPECT_EQ(sidebar->essentials[""][0].title, u"Calendar");
  ASSERT_EQ(sidebar->essentials["Profile 2"].size(), 1u);
  EXPECT_EQ(sidebar->essentials["Profile 2"][0].title, u"Slack");
}

TEST(ArcSidebarTest, TodayTabsAreLeftOut) {
  std::optional<ArcSidebar> sidebar = ParseArcSidebar(kSidebar);
  ASSERT_TRUE(sidebar);
  for (const ArcSpace& space : sidebar->spaces) {
    for (const ArcNode& node : space.pinned) {
      EXPECT_NE(node.title, u"Today");
    }
  }
}

TEST(ArcSidebarTest, NotArcJsonIsRefused) {
  EXPECT_FALSE(ParseArcSidebar("{}"));
  EXPECT_FALSE(ParseArcSidebar("not json"));
  EXPECT_FALSE(ParseArcSidebar(R"({"sidebar": {"containers": []}})"));
}

}  // namespace
}  // namespace cromarc
