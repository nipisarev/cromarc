// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/ports/browser_frame.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/vertical_strip.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/spaces/space_pref_names.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/compact_mode.h"
#include "cromarc/browser/ui/sidebar/space_input.h"
#include "cromarc/browser/ui/welcome/welcome_ui.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
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

// static
bool SidebarView::ToggleCompactMode(BrowserWindowInterface* browser) {
  SidebarView* sidebar = ForBrowser(browser);
  return sidebar && sidebar->compact_mode()->Toggle();
}

SidebarView::Host::Host() = default;
SidebarView::Host::Host(Host&&) = default;
SidebarView::Host& SidebarView::Host::operator=(Host&&) = default;
SidebarView::Host::~Host() = default;

SidebarView::SidebarView(Host host)
    : views::WebView(host.browser_context),
      browser_(host.browser),
      frame_(std::move(host.frame)),
      strip_(std::move(host.strip)),
      tabs_(*host.tabs) {
  Sidebars().insert(this);
  compact_mode_ = std::make_unique<CompactMode>(
      *strip_, *frame_, *tabs_, browser_,
      base::BindRepeating(&WatchMouseEverywhere));
  InstallSpaceInputMonitor(*host.windows);
  SpaceController::Get()->OnWindowCreated(browser_);
  MaybeShowWelcome(browser_);
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
          content::WebContents::CreateParams(host.browser_context));
  // Before the load: SidebarUI looks the window up when the page binds
  // TabStripService.
  frame_->AttachWebUI(contents.get());
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  // The page marks the empty space that moves the window (app-region: drag),
  // as Zen's draggable sidebar (zen.view.draggable-sidebar).
  contents->SetSupportsDraggableRegions(true);
  SetOwnedWebContents(std::move(contents));
  theme_pref_.Init(host.prefs);
  theme_pref_.Add(kSpaceThemePref,
                  base::BindRepeating(&BrowserFrame::RepaintBackground,
                                      base::Unretained(frame_.get())));
  web_contents()->GetController().LoadURL(
      GURL(kSidebarURL), content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

SidebarView::~SidebarView() {
  Sidebars().erase(this);
}

void SidebarView::ShareSidebarState() {
  if (!shared_sidebar_state_) {
    shared_sidebar_state_ = SpaceController::Get()->ShareSidebarState(*strip_);
  }
}

void SidebarView::SetThemePreview(std::optional<std::string> theme) {
  theme_preview_ = std::move(theme);
  frame_->RepaintBackground();
}

void SidebarView::SetThemeBlend(std::optional<Blend> blend) {
  if (!blend && !theme_blend_) {
    return;
  }
  theme_blend_ = std::move(blend);
  frame_->RepaintBackground();
}

void SidebarView::SetLibraryWidth(int width) {
  if (library_width_ == width) {
    return;
  }
  library_width_ = width;
  // The region asks for it from its preferred size.
  PreferredSizeChanged();
}

gfx::Insets SidebarView::ColumnInsets() const {
  const std::optional<gfx::Rect> region = strip_->BoundsInScreen();
  if (!region || library_width_ > 0) {
    return gfx::Insets();
  }
  const gfx::Rect own = GetBoundsInScreen();
  return gfx::Insets::TLBR(std::max(0, own.y() - region->y()), 0,
                           std::max(0, region->bottom() - own.bottom()), 0);
}

void SidebarView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  // In order: a later region wins where they overlap, as app-region does.
  drag_region_.setEmpty();
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    drag_region_.op(gfx::RectToSkIRect(region->bounds),
                    region->draggable ? SkRegion::kUnion_Op
                                      : SkRegion::kDifference_Op);
  }
}

bool SidebarView::IsDragRegion(const gfx::Point& point_in_view) const {
  return GetVisible() && HitTestPoint(point_in_view) &&
         drag_region_.contains(point_in_view.x(), point_in_view.y());
}

gfx::Size SidebarView::WindowButtons() const {
  return frame_->WindowButtons();
}

void SidebarView::ShowNavigationHistory(bool forward,
                                        const gfx::Point& point_in_page) {
  gfx::Point point = point_in_page;
  views::View::ConvertPointToScreen(this, &point);
  frame_->ShowNavigationHistory(forward, point);
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
  fullscreen_subscription_ = frame_->OnFullscreenChanged(base::BindRepeating(
      &BrowserFrame::UpdatePageCorners, base::Unretained(frame_.get())));
  frame_->UpdatePageCorners();
}

void SidebarView::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace cromarc
