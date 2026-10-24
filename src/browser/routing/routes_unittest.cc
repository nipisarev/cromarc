// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/routing/routes.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

Route Rule(const std::string& reference,
           Route::Match match,
           const std::string& space) {
  Route route;
  route.id = reference;
  route.reference = reference;
  route.match = match;
  route.space = space;
  return route;
}

TEST(RoutesTest, ContainsIgnoresCase) {
  const Route route = Rule("GitHub.com/qase", Route::Match::kContains, "Work");
  EXPECT_TRUE(RouteMatches(route, GURL("https://github.com/qase-tms/app")));
  EXPECT_FALSE(RouteMatches(route, GURL("https://github.com/nipisarev")));
}

TEST(RoutesTest, EqualToIgnoresSchemeWwwAndTrailingSlash) {
  const Route route = Rule("example.com/a", Route::Match::kEqualTo, "Work");
  EXPECT_TRUE(RouteMatches(route, GURL("https://www.example.com/a/")));
  EXPECT_TRUE(RouteMatches(route, GURL("http://example.com/a")));
  EXPECT_FALSE(RouteMatches(route, GURL("https://example.com/a/b")));
}

TEST(RoutesTest, RegexIsCaseInsensitiveAndBadOnesNeverMatch) {
  EXPECT_TRUE(RouteMatches(
      Rule(R"(^https://[a-z]+\.atlassian\.net/)", Route::Match::kRegex, "W"),
      GURL("https://QASE.atlassian.net/browse/X-1")));
  EXPECT_FALSE(RouteMatches(Rule("(unclosed", Route::Match::kRegex, "W"),
                            GURL("https://unclosed.example")));
}

TEST(RoutesTest, EmptyReferenceNeverMatches) {
  EXPECT_FALSE(RouteMatches(Rule("  ", Route::Match::kContains, "W"),
                            GURL("https://example.com")));
}

TEST(RoutesTest, FirstMatchingRuleWinsElseTheDefault) {
  Routes routes;
  routes.routes = {Rule("slack.com", Route::Match::kContains, "Work"),
                   Rule("slack.com/personal", Route::Match::kContains, "Me")};
  routes.default_external = "Default";
  EXPECT_EQ(SpaceForExternalLink(routes, GURL("https://slack.com/personal")),
            "Work");
  EXPECT_EQ(SpaceForExternalLink(routes, GURL("https://news.example")),
            "Default");
  routes.default_external.clear();
  EXPECT_EQ(SpaceForExternalLink(routes, GURL("https://news.example")), "");
}

TEST(RoutesTest, JsonRoundTripsInZensFormat) {
  const std::optional<base::Value> json = base::JSONReader::Read(R"({
    "routes": [
      {"id": "r1", "reference": "jira", "matchType": "contains", "openIn": "Work"},
      {"id": "r2", "reference": "^x", "matchType": "regex", "openIn": "Me"},
      {"id": "bad", "reference": "y", "matchType": "glob", "openIn": "Me"}
    ],
    "defaultRouteExternal": "most-recent-space"
  })", base::JSON_PARSE_RFC);
  ASSERT_TRUE(json);
  std::optional<Routes> routes = RoutesFromValue(*json);
  ASSERT_TRUE(routes);
  ASSERT_EQ(routes->routes.size(), 2u);
  EXPECT_EQ(routes->routes[1].match, Route::Match::kRegex);
  EXPECT_EQ(routes->default_external, "");
  const base::Value back = RoutesToValue(*routes);
  std::optional<Routes> again = RoutesFromValue(back);
  ASSERT_TRUE(again);
  EXPECT_EQ(again->routes.size(), 2u);
  EXPECT_EQ(*back.GetDict().FindString("defaultRouteExternal"),
            "most-recent-space");
}

}  // namespace
}  // namespace cromarc
