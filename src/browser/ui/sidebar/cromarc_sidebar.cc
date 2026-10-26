// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"

#include "cromarc/browser/ui/sidebar/sidebar_view.h"

namespace cromarc {

int RegionWidth(BrowserWindowInterface* browser, int width) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser);
  return sidebar && sidebar->library_width() > 0 ? sidebar->library_width()
                                                 : width;
}

bool LibraryTakesRegion(BrowserWindowInterface* browser) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser);
  return sidebar && sidebar->library_width() > 0;
}

gfx::Insets CardInsets(bool compact) {
  return gfx::Insets::TLBR(0, compact ? kPageSeparation : 0, kPageSeparation,
                           kPageSeparation);
}

}  // namespace cromarc
