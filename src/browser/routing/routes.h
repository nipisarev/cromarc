// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ROUTING_ROUTES_H_
#define CROMARC_BROWSER_ROUTING_ROUTES_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"

class GURL;

namespace cromarc {

// A rule sending links to a space, as Zen's space routing has them
// (zen:space-routing/ZenSpaceRoutingManager.sys.mjs).
struct Route {
  enum class Match { kContains, kEqualTo, kRegex };

  std::string id;
  std::string reference;
  Match match = Match::kContains;
  // The space (profile directory name) the link opens in.
  std::string space;
};

struct Routes {
  Routes();
  Routes(const Routes&);
  Routes& operator=(const Routes&);
  ~Routes();

  std::vector<Route> routes;
  // Where links from other apps that match no rule go; "" is the space used
  // last (Zen's "most-recent-space").
  std::string default_external;
};

// Zen's matchType names: "contains", "equal-to", "regex".
std::string_view MatchName(Route::Match match);
std::optional<Route::Match> MatchFromName(std::string_view name);

// Zen's isRouteMatching: case-insensitive; "equal-to" ignores the scheme,
// "www." and a trailing slash.
bool RouteMatches(const Route& route, const GURL& url);

// The space for a link from another app: the first matching rule's, else
// the default; "" for the space used last.
std::string SpaceForExternalLink(const Routes& routes, const GURL& url);

// The routes file's JSON: {"routes": [{id, reference, matchType, openIn}],
// "defaultRouteExternal": ...}, Zen's own field names.
std::optional<Routes> RoutesFromValue(const base::Value& value);
base::Value RoutesToValue(const Routes& routes);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ROUTING_ROUTES_H_
