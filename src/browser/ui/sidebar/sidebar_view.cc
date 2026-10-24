// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_view.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/compact_mode.h"
#include "cromarc/browser/ui/sidebar/space_input.h"
#include "cromarc/browser/ui/welcome/welcome_ui.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

std::set<SidebarView*>& Sidebars() {
  static base::NoDestructor<std::set<SidebarView*>> sidebars;
  return *sidebars;
}

}  // namespace

// static
SidebarView* SidebarView::FindAt(gfx::NativeWindow window,
                                 const gfx::Point& screen_point) {
  for (SidebarView* sidebar : Sidebars()) {
    views::Widget* widget = sidebar->GetWidget();
    if (widget && widget->GetNativeWindow() == window &&
        sidebar->IsDrawn() &&
        sidebar->GetBoundsInScreen().Contains(screen_point)) {
      return sidebar;
    }
  }
  return nullptr;
}

// static
BrowserWindowInterface* SidebarView::BrowserFor(gfx::NativeWindow window) {
  SidebarView* sidebar = ForWindow(window);
  return sidebar ? sidebar->browser() : nullptr;
}

// static
SidebarView* SidebarView::ForWindow(gfx::NativeWindow window) {
  for (SidebarView* sidebar : Sidebars()) {
    views::Widget* widget = sidebar->GetWidget();
    if (widget && widget->GetNativeWindow() == window) {
      return sidebar;
    }
  }
  return nullptr;
}

// static
SidebarView* SidebarView::ForBrowser(BrowserWindowInterface* browser) {
  for (SidebarView* sidebar : Sidebars()) {
    if (sidebar->browser() == browser) {
      return sidebar;
    }
  }
  return nullptr;
}

SidebarView::SidebarView(BrowserWindowInterface* browser)
    : views::WebView(browser->GetProfile()), browser_(browser) {
  Sidebars().insert(this);
  compact_mode_ = std::make_unique<CompactMode>(this);
  InstallSpaceInputMonitor();
  SpaceController::Get()->OnWindowCreated(browser);
  MaybeShowWelcome(browser);
  // Takes all the height left in the region's content area. The bottom button
  // container after it is unbounded too, with weight 1: at equal weights
  // FlexLayout splits the space. Zero-weight children are served first, so
  // weight 0 gives the space to this view. It must not have a zero preferred
  // size: FlexLayout serves those last.
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded)
                  .WithWeight(0));
  SetPreferredSize(gfx::Size(0, 1));

  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser->GetProfile()));
  // Before the load: SidebarUI looks the window up when the page binds
  // TabStripService.
  webui::SetBrowserWindowInterface(contents.get(), browser);
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  SetOwnedWebContents(std::move(contents));
  web_contents()->GetController().LoadURL(
      GURL(kSidebarURL), content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

SidebarView::~SidebarView() {
  Sidebars().erase(this);
}

bool SidebarView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace cromarc
