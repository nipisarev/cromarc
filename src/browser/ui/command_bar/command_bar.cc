// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/command_bar/command_bar.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/ui/command_bar/command_bar_page_handler.h"
#include "cromarc/browser/ui/browser_commands.h"
#include "cromarc/browser/ui/command_bar/cromarc_command_bar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// Panel size; the page draws its own rounded card and shadow inside.
constexpr int kWidth = 680;
constexpr int kHeight = 480;

std::map<BrowserWindowInterface*, std::unique_ptr<CommandBar>>& Bars() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, std::unique_ptr<CommandBar>>>
      bars;
  return *bars;
}

}  // namespace

bool ShowCommandBar(Tabs& tabs,
                    Windows& windows,
                    BrowserWindowInterface* browser,
                    bool edit_current_url) {
  if (!windows.IsNormal(browser) || windows.IsOffTheRecord(browser)) {
    return false;
  }
  CommandBar::GetOrCreate(tabs, windows, browser)->Show(edit_current_url);
  return true;
}

// static
CommandBar* CommandBar::GetOrCreate(Tabs& tabs,
                                    Windows& windows,
                                    BrowserWindowInterface* browser) {
  auto& bars = Bars();
  auto it = bars.find(browser);
  if (it == bars.end()) {
    it = bars.emplace(browser,
                      std::make_unique<CommandBar>(tabs, windows, browser))
             .first;
  }
  return it->second.get();
}

// static
CommandBar* CommandBar::Get(BrowserWindowInterface* browser) {
  auto it = Bars().find(browser);
  return it == Bars().end() ? nullptr : it->second.get();
}

CommandBar::CommandBar(Tabs& tabs,
                       Windows& windows,
                       BrowserWindowInterface* browser)
    : tabs_(tabs), windows_(windows), browser_(browser) {
  browser_did_close_ = windows_->OnClosed(
      browser, base::BindRepeating([](BrowserWindowInterface* closed) {
        // Destroys this CommandBar.
        Bars().erase(closed);
      }));
}

CommandBar::~CommandBar() {
  // The widget owns both; the handler can't clear itself here, since this bar
  // is already out of Bars().
  web_view_ = nullptr;
  page_handler_ = nullptr;
  widget_observation_.Reset();
  widget_.reset();
}

std::u16string CommandBar::InitialText() const {
  if (!edit_current_url_) {
    return std::u16string();
  }
  const std::optional<TabId> tab = tabs_->GetActive(browser_);
  content::WebContents* contents = tab ? tabs_->GetContents(*tab) : nullptr;
  return contents ? base::UTF8ToUTF16(contents->GetVisibleURL().spec())
                  : std::u16string();
}

void CommandBar::SetPageHandler(CommandBarPageHandler* handler) {
  page_handler_ = handler;
}

gfx::Rect CommandBar::Bounds() const {
  const gfx::Rect window = windows_->BoundsOf(browser_);
  const int width = std::min(kWidth, window.width() - 40);
  return gfx::Rect(window.x() + (window.width() - width) / 2,
                   window.y() + window.height() / 6, width, kHeight);
}

void CommandBar::CreateWidget() {
  content::BrowserContext* browser_context =
      windows_->BrowserContextOf(browser_);
  auto web_view = std::make_unique<views::WebView>(browser_context);
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context));
  // Before the load: CommandBarUI looks the window up when the page binds.
  windows_->AttachWebUI(browser_, contents.get());
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetOwnedWebContents(std::move(contents));
  web_view->set_allow_accelerators(true);
  web_view->web_contents()->GetController().LoadURL(
      GURL(kCommandBarURL), content::Referrer(),
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
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.bounds = Bounds();
  params.name = "CromarcCommandBar";
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_observation_.Observe(widget_.get());
}

void CommandBar::Show(bool edit_current_url) {
  edit_current_url_ = edit_current_url;
  if (!widget_) {
    CreateWidget();
  }
  widget_->SetBounds(Bounds());
  widget_->Show();
  widget_->Activate();
  web_view_->RequestFocus();
  web_view_->web_contents()->Focus();
  if (page_handler_) {
    page_handler_->NotifyShown(InitialText());
  }
}

void CommandBar::Hide() {
  if (widget_ && widget_->IsVisible()) {
    widget_->Hide();
  }
}

void CommandBar::OnWidgetActivationChanged(views::Widget* widget,
                                           bool active) {
  // Clicking anywhere else dismisses the bar, as in Arc.
  if (!active) {
    Hide();
  }
}

void CommandBar::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  web_view_ = nullptr;
  page_handler_ = nullptr;
}

}  // namespace cromarc
