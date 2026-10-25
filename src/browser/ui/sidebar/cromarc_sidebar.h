// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
#define CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_

#include <optional>

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

class BrowserView;
class BrowserWindowInterface;
class SkPath;

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace cromarc {

inline constexpr char kSidebarHost[] = "cromarc-sidebar.top-chrome";
inline constexpr char kSidebarURL[] = "chrome://cromarc-sidebar.top-chrome/";

// Zen's zen.theme.content-element-separation, and the page's radius on macOS
// (zen-browser-container.css): --zen-native-inner-radius over the squircle,
// max(5px, (11px - 8px / 2) * 1.3) / 1.3, --zen-border-radius being 11px from
// Tahoe on (zenThemeModifier.js).
inline constexpr int kPageSeparation = 8;
inline constexpr float kPageCornerRadius = 7;

// Hook H2, called by the vertical tab strip region right after it adds its
// native tab list to `parent`: hides that list, the New Tab button under it,
// the line and the tab groups / tab search buttons above it, and puts the
// Cromarc sidebar after the list. Safe to call again when the region re-adds
// its list.
void InstallSidebar(views::View* parent,
                    views::View* native_tab_list,
                    BrowserWindowInterface* browser);

// Hook H16, from the vertical tab strip region's preferred size: while the
// Library is out the column is as wide as the Library wants (Zen's is at least
// 418px and moves the page aside), otherwise `width`.
int RegionWidth(BrowserWindowInterface* browser, int width);

// Hook H11, called by MultiContentsView's layout outside split view: the
// space round the page in a window with the sidebar, so the page is a card on
// the window's background (Zen's element separation). None in tab fullscreen.
gfx::Insets PageInsets(BrowserView* browser_view);

// Hook H18, from the browser view's layout: the page's corners, all round
// where it is a card, otherwise `chrome_corners`.
gfx::RoundedCornersF PageCorners(BrowserView* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners);

// Hook H18, from ThemedBackground: true when `view` paints nothing, the space
// theme under it showing, as Zen's toolbox does (the bookmarks bar).
bool PaintsOnSpaceTheme(const views::View* view,
                        const BrowserView* browser_view);

// Hook H14: Zen draws the space theme (its gradient and grain) under the whole
// window and Chrome's frame, toolbar and page backgrounds stay clear over it,
// glass included. True for normal windows, the ones with the sidebar.
bool ThemesWindow(const BrowserView* browser_view);

// Hook H14, from the frame view's paint: the space theme over `bounds`.
void PaintWindowBackground(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           BrowserView* browser_view);

// Hook H15, from GlassFrameService: space windows keep their glass whether
// activated or not, so the window a space switch brings in has it already.
bool KeepsGlass(BrowserWindowInterface* browser);

// Hook H14, from CustomCornersBackground (toolbar, top container, tab strip
// region): true when Cromarc paints `view`'s background inside `path`
// instead, which is nothing but for the sidebar floating over the page in
// compact mode, which gets Zen's opaque toolbar background.
bool PaintChromeBackground(gfx::Canvas* canvas,
                           const views::View* view,
                           const SkPath& path,
                           const BrowserView* browser_view);

// Hook H17, from CustomCornersBackground::SetUseBackgroundBlur(): the blur of
// what `view` floats over, nothing to leave it to Chrome. The sidebar out
// over the page in compact mode is acrylic (zen.theme.acrylic-sidebar).
std::optional<float> ChromeBackgroundBlur(const views::View* view,
                                          const BrowserView* browser_view);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_CROMARC_SIDEBAR_H_
