// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/windows_adapter.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/navigator/browser_navigator_params.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"  // nogncheck: as above
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace cromarc {

WindowsAdapter::WindowsAdapter() = default;
WindowsAdapter::~WindowsAdapter() = default;

Profile* WindowsAdapter::ProfileOf(BrowserWindowInterface* window) {
  return window->GetProfile();
}

content::BrowserContext* WindowsAdapter::BrowserContextOf(
    BrowserWindowInterface* window) {
  return window->GetProfile();
}

bool WindowsAdapter::IsOffTheRecord(BrowserWindowInterface* window) {
  return window->GetProfile()->IsOffTheRecord();
}

bool WindowsAdapter::IsNormal(BrowserWindowInterface* window) {
  return window->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

WindowId WindowsAdapter::IdOf(BrowserWindowInterface* window) {
  return WindowId(window->GetSessionID().id());
}

BrowserWindowInterface* WindowsAdapter::FromId(WindowId id) {
  return BrowserWindowInterface::FromSessionID(
      SessionID::FromSerializedValue(id.value()));
}

std::vector<BrowserWindowInterface*> WindowsAdapter::NormalWindowsOf(
    Profile* profile) {
  std::vector<BrowserWindowInterface*> windows;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() == profile &&
        browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
      windows.push_back(browser);
    }
  }
  return windows;
}

bool WindowsAdapter::IsOnScreen(BrowserWindowInterface* window) {
  const ui::BaseWindow* base_window = window->GetWindow();
  return base_window && base_window->IsVisible() &&
         !base_window->IsMinimized();
}

gfx::Rect WindowsAdapter::BoundsOf(BrowserWindowInterface* window) {
  return window->GetWindow()->GetBounds();
}

gfx::NativeWindow WindowsAdapter::NativeWindowOf(
    BrowserWindowInterface* window) {
  return window->GetWindow()->GetNativeWindow();
}

void WindowsAdapter::Activate(BrowserWindowInterface* window) {
  window->GetWindow()->Activate();
}

void WindowsAdapter::Close(BrowserWindowInterface* window) {
  window->GetWindow()->Close();
}

base::CallbackListSubscription WindowsAdapter::OnClosed(
    BrowserWindowInterface* window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
  return window->RegisterBrowserDidClose(std::move(callback));
}

BrowserWindowInterface* WindowsAdapter::OpenPopup(Profile* profile,
                                                  const gfx::Rect& bounds,
                                                  const GURL& url) {
  BrowserWindowCreateParams create(BrowserWindowInterface::TYPE_POPUP, *profile,
                                   /*from_user_gesture=*/true);
  create.initial_bounds = bounds;
  create.initial_origin_specified =
      BrowserWindowCreateParams::ValueSpecified::kSpecified;
  create.omit_from_session_restore = true;
  create.should_trigger_session_restore = false;
  BrowserWindowInterface* window = CreateBrowserWindow(std::move(create));
  if (!window) {
    return nullptr;
  }
  // An empty popup takes the tab.
  NavigateParams params(window, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = true;
  Navigate(&params);
  return params.browser == window ? window : nullptr;
}

void WindowsAdapter::AttachWebUI(BrowserWindowInterface* window,
                                 content::WebContents* contents) {
  webui::SetBrowserWindowInterface(contents, window);
}

void WindowsAdapter::AddToToolbar(BrowserWindowInterface* window,
                                  std::unique_ptr<views::View> view) {
  if (BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(window)) {
    browser_view->toolbar()->AddChildView(std::move(view));
  }
}

}  // namespace cromarc
