// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/sidebar_extensions.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"  // nogncheck: as above
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace cromarc {

SidebarExtensions::SidebarExtensions(BrowserWindowInterface* browser)
    : browser_(browser) {}

SidebarExtensions::~SidebarExtensions() {
  GiveBack();
}

views::View* SidebarExtensions::Container() const {
  if (taken_) {
    return taken_;
  }
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser_);
  ToolbarView* toolbar = view ? view->toolbar() : nullptr;
  return toolbar ? toolbar->extensions_container() : nullptr;
}

void SidebarExtensions::Place(const gfx::Rect& bounds_in_screen) {
  if (bounds_in_screen.IsEmpty()) {
    if (widget_) {
      widget_->Hide();
    }
    return;
  }
  Take();
  if (!widget_) {
    return;
  }
  widget_->SetBounds(bounds_in_screen);
  widget_->ShowInactive();
}

int SidebarExtensions::PreferredWidth() const {
  views::View* container = Container();
  return container ? container->GetPreferredSize().width() : 0;
}

base::CallbackListSubscription SidebarExtensions::OnWidthChanged(
    base::RepeatingClosure callback) {
  return width_changed_.Add(std::move(callback));
}

void SidebarExtensions::Take() {
  if (taken_) {
    return;
  }
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser_);
  ToolbarView* toolbar = view ? view->toolbar() : nullptr;
  views::View* container = toolbar ? toolbar->extensions_container() : nullptr;
  if (!container || container->parent() != toolbar || !view->GetWidget()) {
    return;
  }
  toolbar_ = toolbar;
  toolbar_observation_.Observe(toolbar);
  toolbar_index_ = toolbar->GetIndexOf(container).value_or(0);
  std::unique_ptr<views::View> owned = toolbar->RemoveChildViewT(container);
  delegate_ = std::make_unique<views::WidgetDelegate>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate_.get();
  params.parent = view->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.name = "CromarcSidebarExtensions";
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  owned->SetVisible(true);
  taken_ = widget_->SetContentsView(std::move(owned));
  window_observation_.Observe(view->GetWidget());
  container_observation_.Observe(taken_);
}

void SidebarExtensions::GiveBack() {
  if (!taken_) {
    return;
  }
  container_observation_.Reset();
  window_observation_.Reset();
  std::unique_ptr<views::View> owned =
      taken_->parent()->RemoveChildViewT(taken_.get());
  taken_ = nullptr;
  if (toolbar_) {
    toolbar_->AddChildViewAt(
        std::move(owned),
        std::min(toolbar_index_, toolbar_->children().size()));
  }
  // With no toolbar left it goes now, while the root view of the widget it
  // was in, which it watches (ToolbarIconContainerView's restore observer),
  // still lives.
  owned.reset();
  toolbar_observation_.Reset();
  toolbar_ = nullptr;
  widget_.reset();
  delegate_.reset();
}

void SidebarExtensions::OnWidgetDestroying(views::Widget* widget) {
  // The toolbar and Chromium's coordinator of the container outlive this
  // only as long as the window: give it back first.
  GiveBack();
}

void SidebarExtensions::OnViewPreferredSizeChanged(views::View* view) {
  width_changed_.Notify();
}

void SidebarExtensions::OnViewIsDeleting(views::View* view) {
  if (view == toolbar_) {
    toolbar_observation_.Reset();
    toolbar_ = nullptr;
  }
}

}  // namespace cromarc
