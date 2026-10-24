// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_view.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/multi_contents_view.h"  // nogncheck: as above
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
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
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

constexpr int kMaxReloadsPerMinute = 3;

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
  // Takes all the height left in the region's content area. Should the
  // bottom button container after it show, it is unbounded too, with weight
  // 1: at equal weights FlexLayout splits the space. Zero-weight children are
  // served first, so weight 0 gives the space to this view. It must not have a
  // zero preferred size: FlexLayout serves those last.
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

void SidebarView::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now - first_reload_ > base::Minutes(1)) {
    first_reload_ = now;
    reloads_ = 0;
  }
  if (++reloads_ > kMaxReloadsPerMinute) {
    LOG(ERROR) << "Cromarc: the sidebar keeps crashing; not reloading it";
    return;
  }
  // Not from inside the observer call: the frame is still being torn down.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SidebarView::Reload, weak_factory_.GetWeakPtr()));
}

void SidebarView::AddedToWidget() {
  views::WebView::AddedToWidget();
  // The window's features, fullscreen among them, come after its views.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SidebarView::ObserveFullscreen,
                                weak_factory_.GetWeakPtr()));
}

void SidebarView::ObserveFullscreen() {
  ExclusiveAccessManager* manager = ExclusiveAccessManager::From(browser_);
  if (!manager) {
    return;
  }
  fullscreen_subscription_ =
      manager->fullscreen_controller()->RegisterOnFullscreenStateChanged(
          base::BindRepeating(&SidebarView::UpdatePageCorners,
                              base::Unretained(this)));
  UpdatePageCorners();
}

void SidebarView::UpdatePageCorners() {
  views::View* view = parent();
  while (view && !views::IsViewClass<BrowserView>(view)) {
    view = view->parent();
  }
  BrowserView* browser_view = views::AsViewClass<BrowserView>(view);
  if (!browser_view || !browser_view->multi_contents_view()) {
    return;
  }
  MultiContentsView* contents = browser_view->multi_contents_view();
  contents->SetBackgroundRadii(PageInsets(browser_view).IsEmpty()
                                   ? gfx::RoundedCornersF()
                                   : gfx::RoundedCornersF(kPageCornerRadius));
  contents->InvalidateLayout();
}

void SidebarView::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace cromarc
