// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_window.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/app_lifecycle.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/little/little_bar.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"

namespace cromarc {

namespace {

// Arc's Little window: a quick look, not a workspace.
constexpr gfx::Size kSize(900, 650);

std::map<WindowId, std::unique_ptr<LittleWindow>>& LittleWindows() {
  static base::NoDestructor<std::map<WindowId, std::unique_ptr<LittleWindow>>>
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
void LittleWindow::Open(Tabs& tabs,
                        Windows& windows,
                        AppLifecycle& app_lifecycle,
                        Profile* profile,
                        const GURL& url) {
  // As in Arc, Little windows do not come back after a restart.
  BrowserWindowInterface* browser =
      windows.OpenPopup(profile, CentredOnCursorScreen(), url);
  if (!browser) {
    return;
  }
  auto window = base::WrapUnique(
      new LittleWindow(tabs, windows, app_lifecycle, browser));
  LittleWindow* const little = window.get();
  LittleWindows()[windows.IdOf(browser)] = std::move(window);
  // The popup's toolbar is a FlexLayout row: the location bar stretches and
  // the bar takes its preferred width at the end.
  windows.AddToToolbar(browser, std::make_unique<LittleBar>(little));
}

// static
LittleWindow* LittleWindow::From(Windows& windows,
                                 BrowserWindowInterface* browser) {
  auto it = LittleWindows().find(windows.IdOf(browser));
  return it == LittleWindows().end() ? nullptr : it->second.get();
}

LittleWindow::LittleWindow(Tabs& tabs,
                           Windows& windows,
                           AppLifecycle& app_lifecycle,
                           BrowserWindowInterface* browser)
    : tabs_(tabs),
      windows_(windows),
      app_lifecycle_(app_lifecycle),
      id_(windows.IdOf(browser)),
      profile_(windows.ProfileOf(browser)),
      default_space_(SpaceController::IdOf(profile_)) {
  if (std::optional<TabId> tab = tabs_->GetActive(browser)) {
    tab_subscription_ = tabs_->OnWillClose(
        *tab, base::BindRepeating(&LittleWindow::OnTabWillClose,
                                  weak_factory_.GetWeakPtr(), *tab));
  }
  close_subscription_ = windows_->OnClosed(
      browser, base::BindRepeating(&LittleWindow::OnWindowClosed,
                                   weak_factory_.GetWeakPtr()));
  closing_all_subscription_ =
      app_lifecycle_->OnClosingAllWindows(base::BindRepeating(
          &LittleWindow::OnClosingAllWindows, weak_factory_.GetWeakPtr()));
}

LittleWindow::~LittleWindow() = default;

std::optional<TabId> LittleWindow::PageTab() const {
  BrowserWindowInterface* little = windows_->FromId(id_);
  return little ? tabs_->GetActive(little) : std::nullopt;
}

GURL LittleWindow::url() const {
  const std::optional<TabId> tab = PageTab();
  content::WebContents* contents = tab ? tabs_->GetContents(*tab) : nullptr;
  return contents ? contents->GetLastCommittedURL() : GURL();
}

void LittleWindow::OpenInSpace(const std::string& space_id) {
  SpaceController::Get()->ShowSpace(
      space_id, base::BindOnce(&LittleWindow::OnSpaceShown,
                               weak_factory_.GetWeakPtr()));
}

void LittleWindow::OnSpaceShown(BrowserWindowInterface* space_window) {
  BrowserWindowInterface* little = windows_->FromId(id_);
  const std::optional<TabId> tab = PageTab();
  if (!space_window || !tab) {
    return;
  }
  moved_ = true;
  if (windows_->ProfileOf(space_window) == windows_->ProfileOf(little)) {
    // The same profile: the page itself moves, state and all. The popup
    // closes with its last tab.
    tabs_->MoveToWindow(*tab, space_window);
    return;
  }
  tabs_->Open(space_window, tabs_->GetContents(*tab)->GetLastCommittedURL(),
              WindowOpenDisposition::NEW_FOREGROUND_TAB);
  windows_->Close(little);
}

void LittleWindow::Close() {
  if (BrowserWindowInterface* little = windows_->FromId(id_)) {
    windows_->Close(little);
  }
}

void LittleWindow::OnTabWillClose(TabId tab) {
  if (moved_) {
    return;
  }
  Archive(tabs_->GetContents(tab), /*session_ending=*/false);
}

void LittleWindow::OnClosingAllWindows(bool closing) {
  if (!closing || moved_ || !app_lifecycle_->IsSessionEnding()) {
    return;
  }
  const std::optional<TabId> tab = PageTab();
  if (tab) {
    moved_ = true;
    Archive(tabs_->GetContents(*tab), /*session_ending=*/true);
  }
}

void LittleWindow::Archive(content::WebContents* contents,
                           bool session_ending) {
  ArchiveService* archive = ArchiveService::Get(profile_);
  if (!contents) {
    return;
  }
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
  LittleWindows().erase(id_);
}

}  // namespace cromarc
