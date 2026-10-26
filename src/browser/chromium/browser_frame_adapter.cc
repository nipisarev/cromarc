// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/browser_frame_adapter.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/multi_contents_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"  // nogncheck: as above
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/prefs/pref_service.h"
#include "cromarc/browser/spaces/space_pref_names.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "cromarc/browser/ui/sidebar/space_theme_painter.h"
#include "ui/base/base_window.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace cromarc {

namespace {

// Tests can't see what the frame paints: with this switch it says.
constexpr char kLogBackgroundSwitch[] = "cromarc-test-log-background";

bool LogsBackground() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kLogBackgroundSwitch);
}

bool IsDark(const views::View* view) {
  const ui::ColorProvider* colors = view->GetColorProvider();
  return colors && color_utils::IsDark(colors->GetColor(ui::kColorFrameActive));
}

// The theme being edited, else the space's.
std::string ThemeJson(BrowserView* browser_view) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser_view->browser());
  return sidebar && sidebar->theme_preview()
             ? *sidebar->theme_preview()
             : browser_view->GetProfile()->GetPrefs()->GetString(
                   kSpaceThemePref);
}

}  // namespace

BrowserFrameAdapter::BrowserFrameAdapter(BrowserWindowInterface* browser)
    : browser_(browser) {}

BrowserFrameAdapter::~BrowserFrameAdapter() = default;

gfx::Rect BrowserFrameAdapter::BoundsInScreen() const {
  return browser_->GetWindow()->GetBounds();
}

void BrowserFrameAdapter::RepaintBackground() {
  if (BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser_)) {
    if (views::Widget* widget = view->GetWidget()) {
      widget->non_client_view()->frame_view()->SchedulePaint();
    }
  }
}

void BrowserFrameAdapter::UpdatePageCorners() {
  if (!ExclusiveAccessManager::From(browser_)) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->multi_contents_view()) {
    return;
  }
  MultiContentsView* contents = browser_view->multi_contents_view();
  contents->SetBackgroundRadii(PageInsets(browser_view).IsEmpty()
                                   ? gfx::RoundedCornersF()
                                   : gfx::RoundedCornersF(kPageCornerRadius));
  contents->InvalidateLayout();
}

base::CallbackListSubscription BrowserFrameAdapter::OnFullscreenChanged(
    base::RepeatingClosure callback) {
  ExclusiveAccessManager* manager = ExclusiveAccessManager::From(browser_);
  if (!manager) {
    return {};
  }
  return manager->fullscreen_controller()->RegisterOnFullscreenStateChanged(
      std::move(callback));
}

void BrowserFrameAdapter::AttachWebUI(content::WebContents* contents) {
  webui::SetBrowserWindowInterface(contents, browser_);
}

gfx::Insets PageInsets(BrowserView* browser_view) {
  BrowserWindowInterface* browser = browser_view->browser();
  ExclusiveAccessManager* manager = ExclusiveAccessManager::From(browser);
  if (!SidebarView::ForBrowser(browser) || !manager ||
      manager->fullscreen_controller()->IsTabFullscreen()) {
    return gfx::Insets();
  }
  auto* controller = tabs::VerticalTabStripStateController::From(browser);
  return CardInsets(controller && controller->IsCollapsed());
}

gfx::RoundedCornersF PageCorners(BrowserView* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners) {
  return PageInsets(browser_view).IsEmpty()
             ? chrome_corners
             : gfx::RoundedCornersF(kPageCornerRadius);
}

bool ThemesWindow(const BrowserView* browser_view) {
  // Normal windows are the ones with the sidebar; asked before it exists too.
  return browser_view && browser_view->GetIsNormalType();
}

void PaintWindowBackground(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           BrowserView* browser_view) {
  if (!ThemesWindow(browser_view)) {
    return;
  }
  // The browser's light or dark, which a space theme can set.
  const bool dark = IsDark(browser_view);
  SidebarView* sidebar = SidebarView::ForBrowser(browser_view->browser());
  const SpaceTheme theme = ParseSpaceTheme(ThemeJson(browser_view));
  const std::optional<SidebarView::Blend> blend =
      sidebar ? sidebar->theme_blend() : std::nullopt;
  if (LogsBackground()) {
    LOG(ERROR) << "cromarc background: colors=" << theme.colors.size()
               << " texture=" << theme.texture
               << " preview=" << (sidebar && sidebar->theme_preview())
               << " blend=" << (blend ? blend->progress : 0);
  }
  if (!blend) {
    PaintSpaceTheme(canvas, bounds, theme, dark);
    return;
  }
  PaintSpaceThemeBlend(canvas, bounds, theme, ParseSpaceTheme(blend->theme),
                       blend->progress, dark);
}

bool KeepsGlass(BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

bool PaintChromeBackground(gfx::Canvas* canvas,
                           const views::View* view,
                           const SkPath& path,
                           const BrowserView* const_browser_view) {
  if (!ThemesWindow(const_browser_view)) {
    return false;
  }
  // CustomCornersBackground holds the browser view as const; its window is
  // read only here.
  BrowserView* browser_view = const_cast<BrowserView*>(const_browser_view);
  // Collapsed is compact mode, where the region only shows floating.
  auto* controller =
      tabs::VerticalTabStripStateController::From(browser_view->browser());
  if (!views::IsViewClass<VerticalTabStripRegionView>(view) || !controller ||
      !controller->IsCollapsed()) {
    return true;
  }
  // In an active window what it floats over shows through, blurred
  // (ChromeBackgroundBlur()).
  const float blur = view->layer() ? view->layer()->background_blur() : 0;
  const bool acrylic = blur > 0 && view->GetWidget()->IsActive();
  gfx::RectF window(view->GetWidget()->GetRootView()->bounds());
  views::View::ConvertRectToTarget(view->GetWidget()->GetRootView(), view,
                                   &window);
  const SpaceTheme theme = ParseSpaceTheme(ThemeJson(browser_view));
  if (LogsBackground()) {
    LOG(ERROR) << "cromarc floating sidebar: colors=" << theme.colors.size()
               << " texture=" << theme.texture << " blur=" << blur;
  }
  PaintFloatingSidebar(canvas, view->GetLocalBounds(), window, path, theme,
                       IsDark(view), acrylic);
  return true;
}

bool PaintsOnSpaceTheme(const views::View* view,
                        const BrowserView* browser_view) {
  return ThemesWindow(browser_view) && views::IsViewClass<BookmarkBarView>(view);
}

std::optional<float> ChromeBackgroundBlur(const views::View* view,
                                          const BrowserView* browser_view) {
  if (!ThemesWindow(browser_view) ||
      !views::IsViewClass<VerticalTabStripRegionView>(view)) {
    return std::nullopt;
  }
  auto* controller = tabs::VerticalTabStripStateController::From(
      const_cast<BrowserView*>(browser_view)->browser());
  if (!controller || !controller->IsCollapsed()) {
    return std::nullopt;
  }
  // backdrop-filter: blur(42px).
  return 42.0f;
}

}  // namespace cromarc
