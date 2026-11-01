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
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

class BrowserView;
class BrowserWindowInterface;
class RenderViewContextMenu;
class RenderViewContextMenuObserver;
class GURL;
class Profile;
class SkPath;
struct StartupTab;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class NavigationThrottleRegistry;
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
class SimpleMenuModel;
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
// Instead of Chrome's glass, Zen's window material under a space window; true
// then.
bool InstallWindowMaterial(BrowserView* browser_view);
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

// H19: while the Library is out it takes the region's whole height: no row of
// top buttons above the sidebar, no padding below it.
gfx::Size RegionTopRowSize(BrowserWindowInterface* browser,
                           const gfx::Size& size);
bool RegionTopRowVisible(BrowserWindowInterface* browser, bool visible);
int RegionBottomPadding(BrowserWindowInterface* browser, int padding);

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

// H21: a click on the Dock icon with no window on screen brings back the last
// active space with its tabs, as in Zen; true when Cromarc did.
bool ReopenSpace(const base::FilePath& profile_path);

// H22: Cromarc sees a tab's navigations before they start: a link from an
// Essential or a pinned tab to another site opens in Glance instead.
void AddNavigationThrottles(content::NavigationThrottleRegistry& registry);

// H23: Cromarc adds to the page's context menu ("Open Link in Glance", search
// in Glance), through Chrome's own menu observers.
void AddContextMenuItems(
    RenderViewContextMenu* menu,
    ui::SimpleMenuModel& model,
    base::ReentrantObserverList<RenderViewContextMenuObserver>::
        UncheckedAndDanglingUntriaged& observers);

}  // namespace cromarc::hooks

#endif  // CROMARC_HOOKS_HOOKS_H_
