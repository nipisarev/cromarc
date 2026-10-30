// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_navigation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

// ZenUrlbarController.trimURL with zen.urlbar.show-domain-only-in-sidebar.
TEST(AddressFieldTextTest, AnHttpsAddressShowsItsHostWithoutWww) {
  EXPECT_EQ(AddressFieldText(GURL("https://www.example.com/a/b?c#d")),
            "example.com");
  EXPECT_EQ(AddressFieldText(GURL("https://docs.example.com/")),
            "docs.example.com");
}

TEST(AddressFieldTextTest, AnHttpsAddressKeepsItsPort) {
  EXPECT_EQ(AddressFieldText(GURL("https://example.com:8443/x")),
            "example.com:8443");
}

TEST(AddressFieldTextTest, AnyOtherAddressShowsWholeWithoutALoneSlash) {
  EXPECT_EQ(AddressFieldText(GURL("http://example.com/")),
            "http://example.com");
  EXPECT_EQ(AddressFieldText(GURL("http://example.com/page")),
            "http://example.com/page");
  EXPECT_EQ(AddressFieldText(GURL("chrome://settings/")), "chrome://settings/");
}

TEST(AddressFieldTextTest, TheNewTabPageShowsNothing) {
  EXPECT_EQ(AddressFieldText(GURL("chrome://newtab/")), "");
  EXPECT_EQ(AddressFieldText(GURL()), "");
}

}  // namespace
}  // namespace cromarc
