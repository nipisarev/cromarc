// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/sidebar_installer.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/frame_separator.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"  // nogncheck: as above
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"  // nogncheck: as above
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"  // nogncheck: as above
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/chromium/browser_frame_adapter.h"
#include "cromarc/browser/chromium/vertical_strip_adapter.h"
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
  SidebarView::Host host;
  host.browser = browser;
  host.browser_context = browser->GetProfile();
  host.prefs = browser->GetProfile()->GetPrefs();
  host.frame = std::make_unique<BrowserFrameAdapter>(browser);
  host.strip = std::make_unique<VerticalStripAdapter>(browser, parent);
  host.tabs = &ChromiumTabs();
  host.windows = &ChromiumWindows();
  parent->AddChildViewAt(std::make_unique<SidebarView>(std::move(host)),
                         *index + 1);
}

}  // namespace cromarc
