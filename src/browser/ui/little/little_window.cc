// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_window.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"  // nogncheck: as above
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/little/little_bar.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"

namespace cromarc {

namespace {

// Arc's Little window: a quick look, not a workspace.
constexpr gfx::Size kSize(900, 650);

std::map<SessionID, std::unique_ptr<LittleWindow>>& Windows() {
  static base::NoDestructor<std::map<SessionID, std::unique_ptr<LittleWindow>>>
      windows;
  return *windows;
}

gfx::Rect CentredOnCursorScreen() {
  display::Screen* screen = display::Screen::Get();
  const gfx::Rect area =
      screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint())
          .work_area();
  gfx::Rect bounds(area.CenterPoint(), gfx::Size());
  bounds.Outset(gfx::Outsets::VH(kSize.height() / 2, kSize.width() / 2));
  bounds.AdjustToFit(area);
  return bounds;
}

}  // namespace

// static
void LittleWindow::Open(Profile* profile, const GURL& url) {
  BrowserWindowCreateParams create(BrowserWindowInterface::TYPE_POPUP, *profile,
                                   /*from_user_gesture=*/true);
  create.initial_bounds = CentredOnCursorScreen();
  create.initial_origin_specified =
      BrowserWindowCreateParams::ValueSpecified::kSpecified;
  // As in Arc, Little windows do not come back after a restart.
  create.omit_from_session_restore = true;
  create.should_trigger_session_restore = false;
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(create));
  if (!browser) {
    return;
  }
  // An empty popup takes the tab.
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = true;
  Navigate(&params);
  if (params.browser != browser) {
    return;
  }
  auto window = base::WrapUnique(new LittleWindow(browser));
  LittleWindow* const little = window.get();
  Windows()[browser->GetSessionID()] = std::move(window);
  // The popup's toolbar is a FlexLayout row: the location bar stretches and
  // the bar takes its preferred width at the end.
  if (BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser)) {
    view->toolbar()->AddChildView(std::make_unique<LittleBar>(little));
  }
}

// static
LittleWindow* LittleWindow::From(BrowserWindowInterface* browser) {
  auto it = Windows().find(browser->GetSessionID());
  return it == Windows().end() ? nullptr : it->second.get();
}

LittleWindow::LittleWindow(BrowserWindowInterface* browser)
    : id_(browser->GetSessionID()),
      default_space_(SpaceController::IdOf(browser->GetProfile())) {
  if (tabs::TabInterface* tab = browser->GetActiveTabInterface()) {
    tab_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &LittleWindow::OnTabWillDetach, weak_factory_.GetWeakPtr()));
  }
  close_subscription_ = browser->RegisterBrowserDidClose(base::BindRepeating(
      &LittleWindow::OnWindowClosed, weak_factory_.GetWeakPtr()));
  closing_all_subscription_ =
      chrome::AddClosingAllBrowsersCallback(base::BindRepeating(
          &LittleWindow::OnClosingAllBrowsers, weak_factory_.GetWeakPtr()));
}

LittleWindow::~LittleWindow() = default;

GURL LittleWindow::url() const {
  BrowserWindowInterface* little = BrowserWindowInterface::FromSessionID(id_);
  tabs::TabInterface* tab = little ? little->GetActiveTabInterface() : nullptr;
  return tab ? tab->GetContents()->GetLastCommittedURL() : GURL();
}

void LittleWindow::OpenInSpace(const std::string& space_id) {
  SpaceController::Get()->ShowSpace(
      space_id, base::BindOnce(&LittleWindow::OnSpaceShown,
                               weak_factory_.GetWeakPtr()));
}

void LittleWindow::OnSpaceShown(BrowserWindowInterface* space_window) {
  BrowserWindowInterface* little = BrowserWindowInterface::FromSessionID(id_);
  tabs::TabInterface* tab = little ? little->GetActiveTabInterface() : nullptr;
  if (!space_window || !tab) {
    return;
  }
  moved_ = true;
  if (space_window->GetProfile() == little->GetProfile()) {
    // The same profile: the page itself moves, state and all. The popup
    // closes with its last tab.
    TabStripModel* from = little->GetTabStripModel();
    std::unique_ptr<tabs::TabModel> detached =
        from->DetachTabAtForInsertion(from->GetIndexOfTab(tab));
    TabStripModel* to = space_window->GetTabStripModel();
    to->InsertDetachedTabAt(to->count(), std::move(detached),
                            AddTabTypes::ADD_ACTIVE);
    return;
  }
  space_window->OpenGURL(tab->GetContents()->GetLastCommittedURL(),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  little->GetWindow()->Close();
}

void LittleWindow::Close() {
  if (BrowserWindowInterface* little =
          BrowserWindowInterface::FromSessionID(id_)) {
    little->GetWindow()->Close();
  }
}

void LittleWindow::OnTabWillDetach(tabs::TabInterface* tab,
                                   tabs::TabInterface::DetachReason reason) {
  if (moved_ || reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  Archive(tab->GetContents(), /*session_ending=*/false);
}

void LittleWindow::OnClosingAllBrowsers(bool closing) {
  if (!closing || moved_ ||
      browser_shutdown::GetShutdownType() !=
          browser_shutdown::ShutdownType::kEndSession) {
    return;
  }
  BrowserWindowInterface* little = BrowserWindowInterface::FromSessionID(id_);
  tabs::TabInterface* tab = little ? little->GetActiveTabInterface() : nullptr;
  if (tab) {
    moved_ = true;
    Archive(tab->GetContents(), /*session_ending=*/true);
  }
}

void LittleWindow::Archive(content::WebContents* contents,
                           bool session_ending) {
  ArchiveService* archive = ArchiveService::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  ArchiveEntry entry;
  entry.url = contents->GetLastCommittedURL();
  entry.title = contents->GetTitle();
  entry.closed_at = base::Time::Now();
  entry.reason = "little";
  if (!archive || !entry.url.is_valid() || entry.url.is_empty()) {
    return;
  }
  if (session_ending) {
    archive->AddAtSessionEnd(entry);
  } else {
    archive->Add(std::move(entry));
  }
}

void LittleWindow::OnWindowClosed(BrowserWindowInterface* browser) {
  // Deletes this.
  Windows().erase(id_);
}

}  // namespace cromarc
