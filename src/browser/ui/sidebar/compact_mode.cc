// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/compact_mode.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"  // nogncheck: as above
#include "cromarc/browser/ui/sidebar/compact_hover.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "cromarc/browser/ui/sidebar/space_input.h"
#include "ui/base/base_window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"

namespace cromarc {

namespace {

// Zen's compact mode timings (prefs/zen/compact-mode.yaml).
constexpr base::TimeDelta kKeepAfterLeave = base::Milliseconds(150);
constexpr base::TimeDelta kFlash = base::Milliseconds(800);
// How close to the window's left edge the cursor reveals the sidebar.
constexpr int kEdge = 6;

}  // namespace

CompactMode::CompactMode(SidebarView* sidebar) : sidebar_(sidebar) {
  sidebar->browser()->GetTabStripModel()->AddObserver(this);
}

CompactMode::~CompactMode() = default;

// static
bool CompactMode::Toggle(BrowserWindowInterface* browser) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser);
  auto* controller = tabs::VerticalTabStripStateController::From(browser);
  if (!sidebar || !controller || !controller->ShouldDisplayVerticalTabs()) {
    return false;
  }
  // Collapsing counts as collapsed: Cmd+S during the animation turns back.
  controller->RequestCollapse(controller->GetCollapseState() ==
                              tabs::VerticalTabStripCollapseState::kExpanded);
  return true;
}

bool CompactMode::IsCompact() const {
  auto* controller =
      tabs::VerticalTabStripStateController::From(sidebar_->browser());
  return controller && controller->IsCollapsed();
}

VerticalTabStripRegionView* CompactMode::Region() const {
  for (views::View* view = sidebar_->parent(); view; view = view->parent()) {
    if (auto* region = views::AsViewClass<VerticalTabStripRegionView>(view)) {
      return region;
    }
  }
  return nullptr;
}

void CompactMode::ObserveRegion() {
  VerticalTabStripRegionView* region = Region();
  if (!region || region_observation_.IsObservingSource(region)) {
    return;
  }
  region_observation_.Reset();
  region_observation_.Observe(region);
  OnViewBoundsChanged(region);
}

void CompactMode::OnViewBoundsChanged(views::View* region) {
  // At no width the strip still showed its top buttons over the page: they
  // paint to layers of their own, which the strip's layer doesn't clip. Not
  // while it has width: its shadow falls outside.
  region->layer()->SetMasksToBounds(region->width() == 0);
}

void CompactMode::OnViewIsDeleting(views::View* region) {
  region_observation_.Reset();
}

void CompactMode::OnMouseMoved(const gfx::Point& screen_point) {
  VerticalTabStripRegionView* region = Region();
  if (!IsCompact() || !region) {
    Hide();
    return;
  }
  const gfx::Rect window = sidebar_->browser()->GetWindow()->GetBounds();
  const gfx::Rect strip = region->GetBoundsInScreen();
  const bool in_strip_band = screen_point.y() >= strip.y() &&
                             screen_point.y() < strip.bottom();
  if (!lock_) {
    if (in_strip_band && screen_point.x() - window.x() <= kEdge) {
      Reveal();
    }
    return;
  }
  if (CompactSidebarStaysOut(window, strip, screen_point)) {
    hide_timer_.Stop();
  } else if (!hide_timer_.IsRunning()) {
    HideAfter(kKeepAfterLeave);
  }
}

void CompactMode::Flash() {
  if (!IsCompact()) {
    return;
  }
  Reveal();
  HideAfter(kFlash);
}

void CompactMode::Reveal() {
  hide_timer_.Stop();
  if (lock_) {
    return;
  }
  if (VerticalTabStripRegionView* region = Region()) {
    lock_ = region->GetExpandOnHoverLock(ExpandOnHoverLockType::kKeepExpanded);
    mouse_watch_ = WatchMouseEverywhere(base::BindRepeating(
        &CompactMode::OnMouseMoved, weak_factory_.GetWeakPtr()));
  }
}

void CompactMode::HideAfter(base::TimeDelta delay) {
  hide_timer_.Start(FROM_HERE, delay,
                    base::BindOnce(&CompactMode::Hide, base::Unretained(this)));
}

void CompactMode::Hide() {
  hide_timer_.Stop();
  lock_.reset();
  // Not from inside the monitor's own handler.
  if (mouse_watch_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(mouse_watch_)));
  }
}

void CompactMode::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted ||
      selection.active_tab_changed()) {
    return;
  }
  Flash();
}

}  // namespace cromarc
