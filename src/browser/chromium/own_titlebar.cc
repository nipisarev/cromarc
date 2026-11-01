// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/own_titlebar.h"

#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/window/hit_test_utils.h"

DEFINE_UI_CLASS_PROPERTY_KEY(int, kCromarcOwnTitlebarHeightKey, 0)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCromarcOwnTitlebarKey, false)

namespace cromarc {

namespace {

// The height the popup being created claims; 0 when none is.
int g_pending_height = 0;

// The title bar's place in the browser view: across its top, over the page's
// margin (H11), its own layer so that it stays over the page's layers.
class OwnTitlebarHost : public views::View, public views::ViewObserver {
  METADATA_HEADER(OwnTitlebarHost, views::View)

 public:
  OwnTitlebarHost(BrowserView* browser_view,
                  int height,
                  std::unique_ptr<views::View> content)
      : height_(height) {
    SetProperty(kCromarcOwnTitlebarKey, true);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(std::move(content));
    observation_.Observe(browser_view);
    OnViewBoundsChanged(browser_view);
  }
  OwnTitlebarHost(const OwnTitlebarHost&) = delete;
  OwnTitlebarHost& operator=(const OwnTitlebarHost&) = delete;
  ~OwnTitlebarHost() override = default;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* browser_view) override {
    SetBounds(0, 0, browser_view->width(), height_);
  }
  void OnViewIsDeleting(views::View* browser_view) override {
    observation_.Reset();
  }

 private:
  const int height_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

BEGIN_METADATA(OwnTitlebarHost)
END_METADATA

views::View* HostOf(BrowserView* browser_view) {
  for (views::View* child : browser_view->children()) {
    if (child->GetProperty(kCromarcOwnTitlebarKey)) {
      return child;
    }
  }
  return nullptr;
}

}  // namespace

ScopedOwnTitlebar::ScopedOwnTitlebar(int height)
    : pending_(&g_pending_height, height) {}

ScopedOwnTitlebar::~ScopedOwnTitlebar() = default;

int OwnTitlebarHeight(BrowserView* browser_view) {
  if (!browser_view) {
    return 0;
  }
  // The window being created is the popup that asked for one: its title bar
  // is settled before its NSWindow is made.
  if (g_pending_height && !browser_view->GetProperty(kCromarcOwnTitlebarHeightKey) &&
      browser_view->browser()->GetType() ==
          BrowserWindowInterface::TYPE_POPUP) {
    browser_view->SetProperty(kCromarcOwnTitlebarHeightKey, g_pending_height);
  }
  return browser_view->GetProperty(kCromarcOwnTitlebarHeightKey);
}

void SetOwnTitlebar(BrowserView* browser_view,
                    std::unique_ptr<views::View> view) {
  const int height = OwnTitlebarHeight(browser_view);
  if (!height || HostOf(browser_view)) {
    return;
  }
  browser_view->AddChildView(
      std::make_unique<OwnTitlebarHost>(browser_view, height, std::move(view)));
}

bool IsOwnTitlebarCaption(BrowserView* browser_view, const gfx::Point& point) {
  if (!browser_view || !browser_view->parent() ||
      !browser_view->GetProperty(kCromarcOwnTitlebarHeightKey)) {
    return false;
  }
  views::View* host = HostOf(browser_view);
  if (!host) {
    return false;
  }
  gfx::Point in_widget(point);
  views::View::ConvertPointToWidget(browser_view->parent(), &in_widget);
  return views::GetHitTestComponent(host, in_widget) == HTCAPTION;
}

}  // namespace cromarc
