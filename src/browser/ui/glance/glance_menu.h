// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_MENU_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_MENU_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "url/gurl.h"

namespace cromarc {

// Zen's Glance in the page's context menu (ZenGlanceManager's
// #insertIntoContextMenu and onSearchSelectCommand).

// Where the menu opens.
struct MenuContext {
  GURL link;
  // A tab of a normal window, not off the record.
  bool in_normal_tab = false;
  bool in_glance = false;
};

// A separator in a menu's list of command ids.
inline constexpr int kMenuSeparator = -1;

// "Open Link in Glance": for a web link in a tab that is no glance.
bool OffersOpenLinkInGlance(const MenuContext& context);

// Where it goes in `commands`: as Zen puts it, before the separator that ends
// the open-link items, the first of which is `first_open_command`. Nullopt
// when the menu has no such items.
std::optional<size_t> OpenLinkInGlanceIndex(const std::vector<int>& commands,
                                            int first_open_command);

// "Search … for …" opens its result as a glance of the tab (Zen's
// zen.glance.enable-contextmenu-search), when it opens in a tab in front.
bool SearchOpensInGlance(const MenuContext& context);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_MENU_H_
