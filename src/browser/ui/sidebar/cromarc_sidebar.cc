// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/frame_separator.h"  // nogncheck: as above
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"  // nogncheck: as above
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"  // nogncheck: as above
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace cromarc {

void InstallSidebar(views::View* parent,
                    views::View* native_tab_list,
                    BrowserWindowInterface* browser) {
  native_tab_list->SetVisible(false);
  for (views::View* child : parent->children()) {
    // Its only button, New Tab, is in the sidebar's list. The line under the
    // top buttons isn't in Zen.
    if (views::IsViewClass<VerticalTabStripBottomContainer>(child) ||
        views::IsViewClass<FrameSeparator>(child)) {
      child->SetVisible(false);
    }
  }
  // Zen's top row holds the window buttons and its compact mode button, our
  // collapse button: tab groups and tab search go (search stays in the
  // command bar).
  for (views::View* child : parent->parent()->children()) {
    if (views::IsViewClass<VerticalTabStripTopContainer>(child)) {
      for (views::View* button : child->children()) {
        if (views::IsViewClass<TabStripComboButton>(button)) {
          button->SetVisible(false);
        }
      }
    }
  }
  for (views::View* child : parent->children()) {
    if (views::IsViewClass<SidebarView>(child)) {
      return;
    }
  }
  const std::optional<size_t> index = parent->GetIndexOf(native_tab_list);
  CHECK(index.has_value());
  parent->AddChildViewAt(std::make_unique<SidebarView>(browser), *index + 1);
}

int RegionWidth(BrowserWindowInterface* browser, int width) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser);
  return sidebar && sidebar->library_width() > 0 ? sidebar->library_width()
                                                 : width;
}

gfx::Insets PageInsets(BrowserView* browser_view) {
  BrowserWindowInterface* browser = browser_view->browser();
  ExclusiveAccessManager* manager = ExclusiveAccessManager::From(browser);
  if (!SidebarView::ForBrowser(browser) || !manager ||
      manager->fullscreen_controller()->IsTabFullscreen()) {
    return gfx::Insets();
  }
  // Zen: margin on the page's far sides, none toward the sidebar and the
  // toolbar; in compact mode, where the sidebar is not there, on its side
  // too (compact-mode/sidebar.inc.css, #zen-tabbox-wrapper).
  auto* controller = tabs::VerticalTabStripStateController::From(browser);
  const bool compact = controller && controller->IsCollapsed();
  return gfx::Insets::TLBR(0, compact ? kPageSeparation : 0, kPageSeparation,
                           kPageSeparation);
}

gfx::RoundedCornersF PageCorners(BrowserView* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners) {
  return PageInsets(browser_view).IsEmpty()
             ? chrome_corners
             : gfx::RoundedCornersF(kPageCornerRadius);
}

}  // namespace cromarc
