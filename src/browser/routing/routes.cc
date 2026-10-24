// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/routing/routes.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

constexpr char kMostRecentSpace[] = "most-recent-space";

// Zen's #normalizeURL: no scheme, no "www.", no trailing slash.
std::string Normalize(std::string_view url) {
  std::string clean(base::TrimWhitespaceASCII(url, base::TRIM_ALL));
  for (std::string_view prefix : {"https://", "http://"}) {
    if (clean.starts_with(prefix)) {
      clean.erase(0, prefix.size());
      break;
    }
  }
  if (clean.starts_with("www.")) {
    clean.erase(0, 4);
  }
  while (clean.ends_with('/')) {
    clean.pop_back();
  }
  return clean;
}

}  // namespace

std::optional<Route::Match> MatchFromName(std::string_view name) {
  if (name == "contains") {
    return Route::Match::kContains;
  }
  if (name == "equal-to") {
    return Route::Match::kEqualTo;
  }
  if (name == "regex") {
    return Route::Match::kRegex;
  }
  return std::nullopt;
}

std::string_view MatchName(Route::Match match) {
  switch (match) {
    case Route::Match::kContains:
      return "contains";
    case Route::Match::kEqualTo:
      return "equal-to";
    case Route::Match::kRegex:
      return "regex";
  }
}

Routes::Routes() = default;
Routes::Routes(const Routes&) = default;
Routes& Routes::operator=(const Routes&) = default;
Routes::~Routes() = default;

bool RouteMatches(const Route& route, const GURL& url) {
  if (base::TrimWhitespaceASCII(route.reference, base::TRIM_ALL).empty()) {
    return false;
  }
  const std::string spec = base::ToLowerASCII(url.spec());
  const std::string reference = base::ToLowerASCII(route.reference);
  switch (route.match) {
    case Route::Match::kContains:
      return spec.find(reference) != std::string::npos;
    case Route::Match::kEqualTo:
      return Normalize(spec) == Normalize(reference);
    case Route::Match::kRegex: {
      RE2::Options options;
      options.set_case_sensitive(false);
      options.set_log_errors(false);
      const RE2 regex(route.reference, options);
      return regex.ok() && RE2::PartialMatch(url.spec(), regex);
    }
  }
}

std::string SpaceForExternalLink(const Routes& routes, const GURL& url) {
  for (const Route& route : routes.routes) {
    if (RouteMatches(route, url)) {
      return route.space;
    }
  }
  return routes.default_external;
}

std::optional<Routes> RoutesFromValue(const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  Routes routes;
  if (const base::ListValue* list = dict->FindList("routes")) {
    for (const base::Value& item : *list) {
      const base::DictValue* rule = item.GetIfDict();
      if (!rule) {
        continue;
      }
      const std::string* id = rule->FindString("id");
      const std::string* reference = rule->FindString("reference");
      const std::string* match = rule->FindString("matchType");
      const std::string* space = rule->FindString("openIn");
      std::optional<Route::Match> parsed =
          match ? MatchFromName(*match) : std::nullopt;
      if (!id || !reference || !space || !parsed) {
        continue;
      }
      routes.routes.push_back({*id, *reference, *parsed, *space});
    }
  }
  const std::string* fallback = dict->FindString("defaultRouteExternal");
  if (fallback && *fallback != kMostRecentSpace) {
    routes.default_external = *fallback;
  }
  return routes;
}

base::Value RoutesToValue(const Routes& routes) {
  base::ListValue list;
  for (const Route& route : routes.routes) {
    list.Append(base::DictValue()
                    .Set("id", route.id)
                    .Set("reference", route.reference)
                    .Set("matchType", MatchName(route.match))
                    .Set("openIn", route.space));
  }
  return base::Value(
      base::DictValue()
          .Set("routes", std::move(list))
          .Set("defaultRouteExternal", routes.default_external.empty()
                                           ? std::string(kMostRecentSpace)
                                           : routes.default_external));
}

}  // namespace cromarc
