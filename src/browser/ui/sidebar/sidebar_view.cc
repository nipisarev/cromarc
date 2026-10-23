// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_view.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace cromarc {

SidebarView::SidebarView(BrowserWindowInterface* browser)
    : views::WebView(browser->GetProfile()) {
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

SidebarView::~SidebarView() = default;

bool SidebarView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace cromarc
