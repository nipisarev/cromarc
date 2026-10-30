// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/hooks/hooks.h"

#include <optional>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_service.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/themes/theme_service_factory.h"  // nogncheck: as above
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"  // nogncheck: as above
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/chromium/browser_frame_adapter.h"
#include "cromarc/browser/chromium/sidebar_installer.h"
#include "cromarc/browser/keyed_services.h"
#include "cromarc/browser/ui/browser_commands.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/webui_registration.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace cromarc::hooks {

void InstallSidebar(views::View* content_area,
                    views::View* tab_list,
                    BrowserWindowInterface* browser) {
  cromarc::InstallSidebar(content_area, tab_list, browser);
}

void RegisterWebUIConfigs(content::WebUIConfigMap& map) {
  cromarc::RegisterWebUIConfigs(map);
}

void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  cromarc::PopulateWebUIInterfaceBrokers(registry);
}

bool OpenExternalLinks(const std::vector<StartupTab>& tabs, Profile* profile) {
  std::vector<GURL> urls;
  for (const StartupTab& tab : tabs) {
    if (tab.is_untrusted_launch) {
      return false;
    }
    urls.push_back(tab.url);
  }
  return cromarc::OpenExternalLinks(ChromiumProfiles(), ChromiumTabs(),
                                    ChromiumWindows(), ChromiumAppLifecycle(),
                                    urls, profile);
}

bool ShowCommandBar(BrowserWindowInterface* browser, bool edit_current_url) {
  return cromarc::ShowCommandBar(ChromiumTabs(), ChromiumWindows(), browser,
                                 edit_current_url);
}

bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser) {
  return cromarc::KeepWindowOnLastTabClose(browser);
}

bool OpenLittleInSpace(BrowserWindowInterface* browser) {
  return cromarc::OpenLittleInSpace(ChromiumWindows(), browser);
}

bool ReopenSpace(const base::FilePath& profile_path) {
  return cromarc::ReopenSpace(profile_path);
}

void EnsureKeyedServiceFactoriesBuilt() {
  cromarc::EnsureKeyedServiceFactoriesBuilt();
}

void OnTargetURL(content::WebContents* source, const GURL& url) {
  cromarc::OnTargetURL(ChromiumTabs(), ChromiumWindows(), source, url);
}

bool PageShowsBackground(BrowserView* browser_view,
                         bool in_split_view,
                         bool chrome_shows) {
  return chrome_shows ||
         (!in_split_view && !cromarc::PageInsets(browser_view).IsEmpty());
}

gfx::Rect PageSpace(BrowserView* browser_view,
                    bool in_split_view,
                    const gfx::Rect& space) {
  gfx::Rect page = space;
  if (!in_split_view) {
    page.Inset(cromarc::PageInsets(browser_view));
  }
  return page;
}

const gfx::VectorIcon& CollapseButtonIcon(const gfx::VectorIcon& chrome_icon) {
  // Zen's compact mode button keeps its sidebar icon either way
  // (zen-icons/icons.css, #zen-toggle-compact-mode).
  return kDockToLeftIcon;
}

void PaintWindowBackground(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           BrowserView* browser_view) {
  cromarc::PaintWindowBackground(canvas, bounds, browser_view);
}

bool ThemesWindow(const BrowserView* browser_view) {
  return cromarc::ThemesWindow(browser_view);
}

bool InstallWindowMaterial(BrowserView* browser_view) {
  return cromarc::InstallWindowMaterial(browser_view);
}

bool PaintChromeBackground(gfx::Canvas* canvas,
                           const views::View* view,
                           const SkPath& path,
                           const BrowserView* browser_view) {
  return cromarc::PaintChromeBackground(canvas, view, path, browser_view);
}

bool ClearSolidLayer(ui::Layer* layer, const BrowserView* browser_view) {
  if (!cromarc::ThemesWindow(browser_view)) {
    return false;
  }
  layer->AsSolidColor()->SetColor(SkColors::kTransparent);
  return true;
}

base::flat_set<raw_ptr<BrowserWindowInterface>> WithSpaceWindows(
    base::flat_set<raw_ptr<BrowserWindowInterface>> chrome_eligible) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (!cromarc::KeepsGlass(browser)) {
      continue;
    }
    // Chrome's own exclusions (GlassFrameService::IsBrowserEligibleForGlass):
    // fullscreen windows, the browser's or a tab's, and extension themes.
    ExclusiveAccessManager* access = ExclusiveAccessManager::From(browser);
    if (access && access->context() && access->context()->IsFullscreen()) {
      continue;
    }
    ThemeService* theme =
        ThemeServiceFactory::GetForProfile(browser->GetProfile());
    if (theme && theme->UsingExtensionTheme()) {
      continue;
    }
    chrome_eligible.insert(browser);
  }
  return chrome_eligible;
}

gfx::Size RegionSize(BrowserWindowInterface* browser, const gfx::Size& size) {
  return gfx::Size(cromarc::RegionWidth(browser, size.width()), size.height());
}

gfx::Size RegionTopRowSize(BrowserWindowInterface* browser,
                           const gfx::Size& size) {
  return cromarc::LibraryTakesRegion(browser) ? gfx::Size(size.width(), 0)
                                              : size;
}

bool RegionTopRowVisible(BrowserWindowInterface* browser, bool visible) {
  return visible && !cromarc::LibraryTakesRegion(browser);
}

int RegionBottomPadding(BrowserWindowInterface* browser, int padding) {
  return cromarc::LibraryTakesRegion(browser) ? 0 : padding;
}

bool SetChromeBackgroundBlur(const views::View* view,
                             const BrowserView* browser_view,
                             ui::Layer* layer,
                             const SkPath& path) {
  const std::optional<float> blur =
      cromarc::ChromeBackgroundBlur(view, browser_view);
  if (!blur) {
    return false;
  }
  layer->SetBackgroundBlur(*blur);
  layer->SetBackdropFilterBounds(path);
  return true;
}

gfx::RoundedCornersF PageCorners(views::View* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners) {
  auto* const view = views::AsViewClass<BrowserView>(browser_view);
  return view ? cromarc::PageCorners(view, chrome_corners) : chrome_corners;
}

bool ShowsTopSeparator(BrowserView* browser_view,
                       bool in_split_view,
                       bool chrome_shows) {
  // A page that is a card has no line over it.
  return chrome_shows &&
         (in_split_view || cromarc::PageInsets(browser_view).IsEmpty());
}

bool PaintsOnSpaceTheme(const views::View* view,
                        const BrowserView* browser_view) {
  return cromarc::PaintsOnSpaceTheme(view, browser_view);
}

}  // namespace cromarc::hooks
