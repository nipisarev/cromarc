// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/theme_picker.h"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/chromium/window_material.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "cromarc/browser/ui/sidebar/theme_picker_page_handler.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// zen-gradient-generator.css: --panel-width; the height until the page says
// its own.
constexpr int kWidth = 380;
constexpr int kInitialHeight = 540;
// openThemePicker: the panel's left edge 10px over the sidebar's right one.
constexpr int kOverlap = 10;

std::map<BrowserWindowInterface*, std::unique_ptr<ThemePicker>>& Pickers() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, std::unique_ptr<ThemePicker>>>
      pickers;
  return *pickers;
}

}  // namespace

// static
ThemePicker* ThemePicker::GetOrCreate(Windows& windows,
                                      BrowserWindowInterface* browser) {
  auto& pickers = Pickers();
  auto it = pickers.find(browser);
  if (it == pickers.end()) {
    it = pickers.emplace(browser, std::make_unique<ThemePicker>(windows, browser))
             .first;
  }
  return it->second.get();
}

// static
ThemePicker* ThemePicker::Get(BrowserWindowInterface* browser) {
  auto it = Pickers().find(browser);
  return it == Pickers().end() ? nullptr : it->second.get();
}

ThemePicker::ThemePicker(Windows& windows, BrowserWindowInterface* browser)
    : windows_(windows), browser_(browser), content_height_(kInitialHeight) {
  browser_did_close_ = windows_->OnClosed(
      browser, base::BindRepeating([](BrowserWindowInterface* closed) {
        // Destroys this ThemePicker.
        Pickers().erase(closed);
      }));
}

ThemePicker::~ThemePicker() {
  // Its window is closing: as Zen's panel hiding with it, the edit is saved.
  if (page_handler_) {
    page_handler_->Commit();
  }
  // The widget owns both; the handler can't clear itself here, since this
  // picker is already out of Pickers().
  web_view_ = nullptr;
  page_handler_ = nullptr;
  widget_observation_.Reset();
  widget_.reset();
}

void ThemePicker::Show(const std::string& space_id) {
  // Open for another space: what was edited first is kept.
  if (IsShowing() && page_handler_) {
    page_handler_->Commit();
  }
  space_id_ = space_id;
  if (!widget_) {
    CreateWidget();
  }
  if (SidebarView* sidebar = SidebarView::ForBrowser(browser_)) {
    strip_hold_ = sidebar->strip().KeepOut();
  }
  widget_->SetBounds(Bounds());
  widget_->Show();
  widget_->Activate();
  web_view_->RequestFocus();
  web_view_->web_contents()->Focus();
  if (page_handler_) {
    page_handler_->NotifyOpened(space_id_);
  }
}

void ThemePicker::Hide() {
  if (!widget_ || !widget_->IsVisible()) {
    return;
  }
  if (page_handler_) {
    page_handler_->Commit();
  }
  widget_->Hide();
  strip_hold_.reset();
}

bool ThemePicker::IsShowing() const {
  return widget_ && widget_->IsVisible();
}

void ThemePicker::SetPageHandler(ThemePickerPageHandler* handler) {
  page_handler_ = handler;
  if (page_handler_ && IsShowing()) {
    page_handler_->NotifyOpened(space_id_);
  }
}

void ThemePicker::SetContentHeight(int height) {
  content_height_ = height;
  if (IsShowing()) {
    widget_->SetBounds(Bounds());
  }
}

gfx::Rect ThemePicker::Bounds() const {
  const gfx::Rect window = windows_->BoundsOf(browser_);
  SidebarView* sidebar = SidebarView::ForBrowser(browser_);
  const std::optional<gfx::Rect> strip =
      sidebar ? sidebar->strip().BoundsInScreen() : std::nullopt;
  const gfx::Rect anchor =
      strip && !strip->IsEmpty() ? *strip : gfx::Rect(window.origin(), {});
  const int height = std::min(content_height_, window.height());
  // Inside the window from the top of the sidebar down; with the compact
  // sidebar away, at the window's edge.
  const int y = std::clamp(anchor.y(), window.y(), window.bottom() - height);
  const int x = std::max(anchor.right() - kOverlap, window.x());
  return gfx::Rect(x, y, kWidth, height);
}

void ThemePicker::CreateWidget() {
  content::BrowserContext* browser_context =
      windows_->BrowserContextOf(browser_);
  auto web_view = std::make_unique<views::WebView>(browser_context);
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context));
  // Before the load: ThemePickerUI looks the window up when the page binds.
  windows_->AttachWebUI(browser_, contents.get());
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetOwnedWebContents(std::move(contents));
  web_view->web_contents()->GetController().LoadURL(
      GURL(kThemePickerURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  delegate_ = std::make_unique<views::WidgetDelegate>();
  web_view_ = delegate_->SetContentsView(std::move(web_view));

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate_.get();
  params.parent =
      views::Widget::GetWidgetForNativeWindow(windows_->NativeWindowOf(browser_))
          ->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
#if BUILDFLAG(IS_MAC)
  params.rounded_corners = gfx::RoundedCornersF(PanelCornerRadius());
#endif
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.bounds = Bounds();
  params.name = "CromarcThemePicker";
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_observation_.Observe(widget_.get());
#if BUILDFLAG(IS_MAC)
  InstallPanelMaterial(widget_->GetNativeWindow());
#endif
}

void ThemePicker::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  // A click anywhere else closes the panel, as Zen's popup.
  if (!active) {
    Hide();
  }
}

void ThemePicker::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  web_view_ = nullptr;
  page_handler_ = nullptr;
}

}  // namespace cromarc
