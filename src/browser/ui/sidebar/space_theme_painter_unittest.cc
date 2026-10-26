// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/space_theme_painter.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

TEST(SpaceThemePainterTest, ReadsTheEditorsTheme) {
  const SpaceTheme theme = ParseSpaceTheme(
      R"({"colors": [[255, 0, 0], [0, 128, 255]], "opacity": 0.6,
          "texture": 0.25})");
  EXPECT_EQ((std::vector<SkColor>{SkColorSetRGB(255, 0, 0),
                                  SkColorSetRGB(0, 128, 255)}),
            theme.colors);
  EXPECT_DOUBLE_EQ(0.6, theme.opacity);
  EXPECT_DOUBLE_EQ(0.25, theme.texture);
}

TEST(SpaceThemePainterTest, NoThemeIsTheDefaults) {
  for (const char* json : {"", "not json", "[]", "{}"}) {
    const SpaceTheme theme = ParseSpaceTheme(json);
    EXPECT_TRUE(theme.colors.empty()) << json;
    EXPECT_DOUBLE_EQ(kDefaultSpaceThemeOpacity, theme.opacity) << json;
    EXPECT_DOUBLE_EQ(0, theme.texture) << json;
  }
}

TEST(SpaceThemePainterTest, KeepsWithinZensLimits) {
  // MIN_OPACITY, MAX_OPACITY and MAX_DOTS of ZenGradientGenerator.mjs.
  const SpaceTheme theme = ParseSpaceTheme(
      R"({"colors": [[1, 1, 1], [2, 2, 2], [3, 3, 3], [4, 4, 4]],
          "opacity": 0.95, "texture": 3})");
  EXPECT_EQ(3u, theme.colors.size());
  EXPECT_DOUBLE_EQ(0.8, theme.opacity);
  EXPECT_DOUBLE_EQ(1, theme.texture);
  EXPECT_DOUBLE_EQ(0.3, ParseSpaceTheme(R"({"opacity": 0.1})").opacity);
}

TEST(SpaceThemePainterTest, SkipsColorsThatAreNotRgb) {
  const SpaceTheme theme =
      ParseSpaceTheme(R"({"colors": [[1, 2], "red", [10, 20, 30]]})");
  EXPECT_EQ(std::vector<SkColor>{SkColorSetRGB(10, 20, 30)}, theme.colors);
}

}  // namespace
}  // namespace cromarc
