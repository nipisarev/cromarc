// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/vertical_strip_adapter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"  // nogncheck: as above
#include "ui/compositor/layer.h"
#include "ui/views/view_utils.h"

namespace cromarc {

namespace {

class HoldAdapter : public VerticalStrip::Hold {
 public:
  explicit HoldAdapter(std::unique_ptr<ExpandOnHoverLock> lock)
      : lock_(std::move(lock)) {}
  ~HoldAdapter() override = default;

 private:
  std::unique_ptr<ExpandOnHoverLock> lock_;
};

}  // namespace

VerticalStripAdapter::VerticalStripAdapter(BrowserWindowInterface* browser,
                                           views::View* inside)
    : browser_(browser), inside_(inside) {
  // Built from hook H2, inside the region, which holds the window's
  // controller: a strip without either would share a made-up state.
  CHECK(controller());
  VerticalTabStripRegionView* region = Region();
  CHECK(region);
  region_observation_.Observe(region);
  OnViewBoundsChanged(region);
}

VerticalStripAdapter::~VerticalStripAdapter() = default;

tabs::VerticalTabStripStateController* VerticalStripAdapter::controller()
    const {
  return tabs::VerticalTabStripStateController::From(browser_);
}

VerticalTabStripRegionView* VerticalStripAdapter::Region() const {
  for (views::View* view = inside_; view; view = view->parent()) {
    if (auto* region = views::AsViewClass<VerticalTabStripRegionView>(view)) {
      return region;
    }
  }
  return nullptr;
}

bool VerticalStripAdapter::IsShown() const {
  return controller() && controller()->ShouldDisplayVerticalTabs();
}

bool VerticalStripAdapter::IsCollapsed() const {
  return controller() && controller()->IsCollapsed();
}

bool VerticalStripAdapter::IsCollapsedOrCollapsing() const {
  return controller() && controller()->GetCollapseState() !=
                             tabs::VerticalTabStripCollapseState::kExpanded;
}

void VerticalStripAdapter::RequestCollapse(bool collapse) {
  if (controller()) {
    controller()->RequestCollapse(collapse);
  }
}

int VerticalStripAdapter::ExpandedWidth() const {
  return controller() ? controller()->GetUncollapsedWidth() : 0;
}

void VerticalStripAdapter::SetExpandedWidth(int width) {
  if (controller()) {
    controller()->SetUncollapsedWidth(width);
  }
}

bool VerticalStripAdapter::IsResizing() const {
  return controller() && controller()->IsResizing();
}

base::CallbackListSubscription VerticalStripAdapter::OnCollapseChanged(
    base::RepeatingClosure callback) {
  if (!controller()) {
    return {};
  }
  return controller()->RegisterOnCollapseChanged(base::BindRepeating(
      [](const base::RepeatingClosure& callback,
         tabs::VerticalTabStripCollapseState) { callback.Run(); },
      std::move(callback)));
}

base::CallbackListSubscription VerticalStripAdapter::OnResizeEnded(
    base::RepeatingClosure callback) {
  if (!controller()) {
    return {};
  }
  return controller()->RegisterOnResizingChanged(base::BindRepeating(
      [](const base::RepeatingClosure& callback, bool resizing) {
        if (!resizing) {
          callback.Run();
        }
      },
      std::move(callback)));
}

std::optional<gfx::Rect> VerticalStripAdapter::BoundsInScreen() const {
  VerticalTabStripRegionView* region = Region();
  if (!region) {
    return std::nullopt;
  }
  return region->GetBoundsInScreen();
}

std::unique_ptr<VerticalStrip::Hold> VerticalStripAdapter::KeepOut() {
  VerticalTabStripRegionView* region = Region();
  if (!region) {
    return nullptr;
  }
  return std::make_unique<HoldAdapter>(
      region->GetExpandOnHoverLock(ExpandOnHoverLockType::kKeepExpanded));
}

void VerticalStripAdapter::OnViewBoundsChanged(views::View* region) {
  // At no width the strip still showed its top buttons over the page: they
  // paint to layers of their own, which the strip's layer doesn't clip. Not
  // while it has width: its shadow falls outside.
  if (region->layer()) {
    region->layer()->SetMasksToBounds(region->width() == 0);
  }
}

void VerticalStripAdapter::OnViewIsDeleting(views::View* region) {
  region_observation_.Reset();
}

}  // namespace cromarc
