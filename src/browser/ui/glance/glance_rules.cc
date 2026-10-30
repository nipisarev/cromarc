// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_rules.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"

namespace cromarc {

bool OpensInGlance(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile();
}

bool LeavesSite(const GURL& page, const GURL& link) {
  if (!link.has_host()) {
    return false;
  }
  const std::string_view link_host = link.host();
  const std::string_view page_host = page.host();
  if (link_host == page_host) {
    return false;
  }
  return link_host.size() < page_host.size()
             ? page_host != base::StrCat({"www.", link_host})
             : link_host != base::StrCat({"www.", page_host});
}

bool OpensAnotherSite(const GURL& opener_page, const GURL& url) {
  if (!OpensInGlance(url)) {
    return false;
  }
  // Zen: a page that is not a site (about:) differs from every site.
  if (!OpensInGlance(opener_page)) {
    return true;
  }
  return opener_page.host() != url.host();
}

bool LeavesForGlance(const LinkNavigation& navigation) {
  return navigation.in_primary_main_frame && navigation.renderer_initiated &&
         navigation.user_gesture && !navigation.same_document &&
         !navigation.reload_or_history && !navigation.post &&
         navigation.tab_kind != TabKind::kToday && !navigation.tab_is_glance &&
         OpensInGlance(navigation.url) &&
         LeavesSite(navigation.page, navigation.url);
}

bool OpensAsGlance(const OpenedTab& tab) {
  return tab.opener_kind != TabKind::kToday && !tab.opener_is_glance &&
         OpensAnotherSite(tab.opener_page, tab.url);
}

}  // namespace cromarc
