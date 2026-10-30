// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_RULES_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_RULES_H_

#include "url/gurl.h"

namespace cromarc {

// What a tab is in its space's sidebar.
enum class TabKind {
  kToday,
  kPinned,
  kEssential,
};

// Zen's isValidGlanceUrl: only web and file pages open in Glance.
bool OpensInGlance(const GURL& url);

// Firefox's rule for app tabs (nsDocShell::ShouldOpenInBlankTarget): a link
// to another host than the page's, a "www." in front of either aside.
bool LeavesSite(const GURL& page, const GURL& link);

// Zen's tabDomainsDiffer, for a tab a page opened: another host, exactly.
bool OpensAnotherSite(const GURL& opener_page, const GURL& url);

// A navigation about to start in a tab.
struct LinkNavigation {
  bool in_primary_main_frame = false;
  bool renderer_initiated = false;
  bool user_gesture = false;
  bool same_document = false;
  bool reload_or_history = false;
  bool post = false;
  TabKind tab_kind = TabKind::kToday;
  bool tab_is_glance = false;
  // What the tab shows, and where the link goes.
  GURL page;
  GURL url;
};

// The user followed a link of an Essential's or a pinned tab's page to
// another site: it opens in Glance instead, and the tab stays on its page
// (Zen's zen.glance.open-essential-external-links on Firefox's app tabs).
bool LeavesForGlance(const LinkNavigation& navigation);

// A tab a page opened, active.
struct OpenedTab {
  TabKind opener_kind = TabKind::kToday;
  bool opener_is_glance = false;
  GURL opener_page;
  GURL url;
};

// Zen's shouldOpenTabInGlance: a page of an Essential or a pinned tab opened
// another site in a tab of its own (target=_blank, window.open). It is the
// opener's glance instead.
bool OpensAsGlance(const OpenedTab& tab);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_RULES_H_
