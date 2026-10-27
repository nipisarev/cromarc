// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/webui_strings.h"

#include <iterator>
#include <set>

#include "cromarc/strings/grit/cromarc_strings_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

TEST(WebUIStringsTest, KeyIsTheMessageNameInLowerCamelCase) {
  EXPECT_EQ(WebUIStringKey("IDS_CROMARC_NEW_TAB"), "newTab");
  EXPECT_EQ(WebUIStringKey("IDS_CROMARC_ICLOUD_ON_AFTER_RESTART"),
            "icloudOnAfterRestart");
  EXPECT_EQ(WebUIStringKey("IDS_CROMARC_LIBRARY_CLEAR_LAST12_HOURS"),
            "libraryClearLast12Hours");
  EXPECT_EQ(WebUIStringKey("IDS_CROMARC_CLOSE"), "close");
}

TEST(WebUIStringsTest, EveryMessageGetsAKeyOfItsOwn) {
  const auto strings = CromarcWebUIStrings();
  EXPECT_EQ(strings.size(), std::size(kCromarcStrings));
  std::set<std::string> keys;
  for (const auto& [key, id] : strings) {
    EXPECT_TRUE(keys.insert(key).second) << key << " is taken twice";
  }
  // Chromium's own keys stay Chromium's.
  EXPECT_FALSE(keys.contains("language"));
  EXPECT_FALSE(keys.contains("textdirection"));
  EXPECT_FALSE(keys.contains("locale"));
}

}  // namespace
}  // namespace cromarc
