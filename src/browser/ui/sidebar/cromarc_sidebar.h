// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
#define CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_

#include "ui/gfx/geometry/insets.h"

class BrowserWindowInterface;

namespace cromarc {

inline constexpr char kSidebarHost[] = "cromarc-sidebar.top-chrome";
inline constexpr char kSidebarURL[] = "chrome://cromarc-sidebar.top-chrome/";
inline constexpr char kThemePickerHost[] = "cromarc-theme-picker.top-chrome";
inline constexpr char kThemePickerURL[] =
    "chrome://cromarc-theme-picker.top-chrome/";

// Zen's zen.theme.content-element-separation, and the page's radius on macOS
// (zen-browser-container.css): --zen-native-inner-radius over the squircle,
// max(5px, (11px - 8px / 2) * 1.3) / 1.3, --zen-border-radius being 11px from
// Tahoe on (zenThemeModifier.js).
inline constexpr int kPageSeparation = 8;
inline constexpr float kPageCornerRadius = 7;

// Hook H16, from the vertical tab strip region's preferred size: while the
// Library is out the column is as wide as the Library wants (Zen's is at least
// 418px and moves the page aside), otherwise `width`.
int RegionWidth(BrowserWindowInterface* browser, int width);

// The space round the page where it is a card on the window's background
// (Zen's element separation): margin on its far sides, none toward the
// sidebar and the toolbar; in compact mode, where the sidebar is not there,
// on its side too (compact-mode/sidebar.inc.css, #zen-tabbox-wrapper).
gfx::Insets CardInsets(bool compact);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
