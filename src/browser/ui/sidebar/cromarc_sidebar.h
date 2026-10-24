// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
#define CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_

class BrowserWindowInterface;

namespace views {
class View;
}  // namespace views

namespace cromarc {

inline constexpr char kSidebarHost[] = "cromarc-sidebar.top-chrome";
inline constexpr char kSidebarURL[] = "chrome://cromarc-sidebar.top-chrome/";

// Hook H2, called by the vertical tab strip region right after it adds its
// native tab list to `parent`: hides that list and the New Tab button under
// it and puts the Cromarc sidebar after the list. Safe to call again when the region re-adds its list.
void InstallSidebar(views::View* parent,
                    views::View* native_tab_list,
                    BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
