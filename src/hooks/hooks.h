// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_HOOKS_HOOKS_H_
#define CROMARC_HOOKS_HOOKS_H_

// Every call Chromium's own files make into Cromarc (patches/hooks/README.md
// has each hook's place and contract). A hook in a Chromium file is one call,
// one guard or one wrapped value on these, nothing more (AGENTS.md, "Hooks").
// Declarations only, so any layer of Chromium can include this header.

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/feature_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

class BrowserView;
class BrowserWindowInterface;
class GURL;
class Profile;
class SkPath;
struct StartupTab;

namespace content {
class WebContents;
class WebUIBrowserInterfaceBrokerRegistry;
class WebUIConfigMap;
}  // namespace content

namespace gfx {
class Canvas;
struct VectorIcon;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace cromarc::hooks {

// H1: new profiles show vertical tabs, where the sidebar lives.
constexpr bool VerticalTabsByDefault(bool /*chrome_default*/) {
  return true;
}

// H2: Chrome added its tab list to the vertical tab strip region's content
// area; the sidebar takes its place.
void InstallSidebar(views::View* content_area,
                    views::View* tab_list,
                    BrowserWindowInterface* browser);

// H3: collapsed (Cromarc's compact mode), the region takes no width.
constexpr int CollapsedVerticalTabStripWidth(int /*chrome_width*/) {
  return 0;
}

// H4: chrome://cromarc-* pages and their Mojo interfaces.
void RegisterWebUIConfigs(content::WebUIConfigMap& map);
void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry);

// H5: URLs from other apps reach Cromarc's routing first; true when it took
// them.
bool OpenExternalLinks(const std::vector<StartupTab>& tabs, Profile* profile);

// H6: commands Cromarc may handle before Chrome; true when it did.
bool ShowCommandBar(BrowserWindowInterface* browser, bool edit_current_url);
bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);
bool OpenLittleInSpace(BrowserWindowInterface* browser);

// H8: Cromarc's keyed services exist before profiles do.
void EnsureKeyedServiceFactoriesBuilt();

// H10: the link under the cursor (Glance's Option+click).
void OnTargetURL(content::WebContents* source, const GURL& url);

// H11: outside split view the page is a card on the window's background.
bool PageShowsBackground(BrowserView* browser_view,
                         bool in_split_view,
                         bool chrome_shows);
gfx::Rect PageSpace(BrowserView* browser_view,
                    bool in_split_view,
                    const gfx::Rect& space);

// H12: the collapse button keeps its sidebar icon in both states.
const gfx::VectorIcon& CollapseButtonIcon(const gfx::VectorIcon& chrome_icon);

// H13: the glass window frame is on by default.
constexpr base::FeatureState GlassFrameDefault(
    base::FeatureState /*chrome_default*/) {
  return base::FEATURE_ENABLED_BY_DEFAULT;
}

// H14: in a space window the theme is painted under the whole window and
// Chrome's frame, toolbar and page backgrounds let it show.
void PaintWindowBackground(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           BrowserView* browser_view);
bool ThemesWindow(const BrowserView* browser_view);
double GlassTint(const BrowserView* browser_view, double chrome_tint);
bool PaintChromeBackground(gfx::Canvas* canvas,
                           const views::View* view,
                           const SkPath& path,
                           const BrowserView* browser_view);
// Clears the page background's solid layer in a space window; true then.
bool ClearSolidLayer(ui::Layer* layer, const BrowserView* browser_view);

// H15: space windows keep the glass frame, active or not, beyond Chrome's
// limit of glass windows.
base::flat_set<raw_ptr<BrowserWindowInterface>> WithSpaceWindows(
    base::flat_set<raw_ptr<BrowserWindowInterface>> chrome_eligible);

// H16: the Library widens the region while it is out.
gfx::Size RegionSize(BrowserWindowInterface* browser, const gfx::Size& size);

// H17: the floating compact sidebar blurs what it is over; true when Cromarc
// set the layer's blur.
bool SetChromeBackgroundBlur(const views::View* view,
                             const BrowserView* browser_view,
                             ui::Layer* layer,
                             const SkPath& path);

// H18: the page as a card all round, no line over it, the bookmarks bar on
// the theme.
gfx::RoundedCornersF PageCorners(views::View* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners);
bool ShowsTopSeparator(BrowserView* browser_view,
                       bool in_split_view,
                       bool chrome_shows);
bool PaintsOnSpaceTheme(const views::View* view,
                        const BrowserView* browser_view);

}  // namespace cromarc::hooks

#endif  // CROMARC_HOOKS_HOOKS_H_
