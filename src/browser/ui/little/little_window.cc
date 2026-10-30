// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_window.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "cromarc/browser/ports/app_lifecycle.h"
#include "cromarc/browser/ports/presence.h"
#include "cromarc/browser/ports/profiles.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"

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
void LittleWindow::Open(const Deps& deps,
                        Profile* profile,
                        const GURL& url,
                        std::optional<gfx::Rect> bounds) {
  // As in Arc, Little windows do not come back after a restart.
  BrowserWindowInterface* browser = deps.windows->OpenPopup(
      profile, bounds ? *bounds : CentredOnCursorScreen(), url);
  if (!browser) {
    return;
  }
  auto window = base::WrapUnique(new LittleWindow(deps, browser));
  LittleWindow* const little = window.get();
  LittleWindows()[deps.windows->IdOf(browser)] = std::move(window);
  // The popup's toolbar is a FlexLayout row: the location bar stretches and
  // the bar takes its preferred width at the end.
  deps.windows->AddToToolbar(browser, deps.delegate->MakeBar(*little));
}

// static
LittleWindow* LittleWindow::From(Windows& windows,
                                 BrowserWindowInterface* browser) {
  auto it = LittleWindows().find(windows.IdOf(browser));
  return it == LittleWindows().end() ? nullptr : it->second.get();
}

LittleWindow::LittleWindow(const Deps& deps, BrowserWindowInterface* browser)
    : deps_(deps),
      id_(deps.windows->IdOf(browser)),
      profile_(deps.windows->ProfileOf(browser)),
      default_space_(deps.profiles->IdOf(profile_)),
      limits_(LimitsFromCommandLine()),
      last_used_(base::Time::Now()) {
  if (std::optional<TabId> tab = deps_.tabs->GetActive(browser)) {
    tab_subscription_ = deps_.tabs->OnWillClose(
        *tab, base::BindRepeating(&LittleWindow::OnTabWillClose,
                                  weak_factory_.GetWeakPtr(), *tab));
  }
  close_subscription_ = deps_.windows->OnClosed(
      browser, base::BindRepeating(&LittleWindow::OnWindowClosed,
                                   weak_factory_.GetWeakPtr()));
  closing_all_subscription_ =
      deps_.app_lifecycle->OnClosingAllWindows(base::BindRepeating(
          &LittleWindow::OnClosingAllWindows, weak_factory_.GetWeakPtr()));
  idle_timer_.Start(
      FROM_HERE,
      std::clamp(limits_.little_idle / 6, base::Seconds(1), base::Minutes(1)),
      base::BindRepeating(&LittleWindow::CheckIdle,
                          weak_factory_.GetWeakPtr()));
}

LittleWindow::~LittleWindow() = default;

std::optional<TabId> LittleWindow::PageTab() const {
  BrowserWindowInterface* little = deps_.windows->FromId(id_);
  return little ? deps_.tabs->GetActive(little) : std::nullopt;
}

GURL LittleWindow::url() const {
  const std::optional<TabId> tab = PageTab();
  return tab ? deps_.tabs->GetLastCommittedUrl(*tab) : GURL();
}

void LittleWindow::OpenInSpace(const std::string& space_id) {
  deps_.delegate->ShowSpace(
      space_id, base::BindOnce(&LittleWindow::OnSpaceShown,
                               weak_factory_.GetWeakPtr()));
}

void LittleWindow::OnSpaceShown(BrowserWindowInterface* space_window) {
  BrowserWindowInterface* little = deps_.windows->FromId(id_);
  const std::optional<TabId> tab = PageTab();
  if (!space_window || !tab) {
    return;
  }
  moved_ = true;
  if (deps_.windows->ProfileOf(space_window) ==
      deps_.windows->ProfileOf(little)) {
    // The same profile: the page itself moves, state and all. The popup
    // closes with its last tab.
    deps_.tabs->MoveToWindow(*tab, space_window);
    return;
  }
  deps_.tabs->Open(space_window, deps_.tabs->GetLastCommittedUrl(*tab),
                   WindowOpenDisposition::NEW_FOREGROUND_TAB);
  deps_.windows->Close(little);
}

void LittleWindow::Close() {
  if (BrowserWindowInterface* little = deps_.windows->FromId(id_)) {
    deps_.windows->Close(little);
  }
}

void LittleWindow::MoveToSpace(const std::string& space_id) {
  BrowserWindowInterface* little = deps_.windows->FromId(id_);
  const GURL page = url();
  if (!little || space_id == default_space_ || page.is_empty()) {
    return;
  }
  const gfx::Rect bounds = deps_.windows->BoundsOf(little);
  deps_.delegate->LoadSpace(
      space_id,
      base::BindOnce(
          [](base::WeakPtr<LittleWindow> window, GURL page, gfx::Rect bounds,
             Profile* profile) {
            if (!window || !profile) {
              return;
            }
            LittleWindow::Open(window->deps_, profile, page, bounds);
            // The page lives on there: this one goes unarchived.
            window->moved_ = true;
            window->Close();
          },
          weak_factory_.GetWeakPtr(), page, bounds));
}

void LittleWindow::CheckIdle() {
  BrowserWindowInterface* little = deps_.windows->FromId(id_);
  if (!little || moved_) {
    return;
  }
  const base::Time now = base::Time::Now();
  base::TimeDelta away = deps_.presence->IdleFor();
  if (deps_.presence->IsScreenLocked()) {
    away = std::max(away, limits_.away);
  }
  if (deps_.windows->IsActive(little) && away < limits_.away) {
    last_used_ = now;
    return;
  }
  if (!LittleIdledOut(last_used_, now, limits_)) {
    return;
  }
  const std::optional<TabId> tab = PageTab();
  if (tab && deps_.tabs->MayLoseInput(*tab)) {
    return;
  }
  Close();
}

void LittleWindow::OnTabWillClose(TabId tab) {
  if (moved_) {
    return;
  }
  Archive(tab, /*session_ending=*/false);
}

void LittleWindow::OnClosingAllWindows(bool closing) {
  if (!closing || moved_ || !deps_.app_lifecycle->IsSessionEnding()) {
    return;
  }
  if (const std::optional<TabId> tab = PageTab()) {
    moved_ = true;
    Archive(*tab, /*session_ending=*/true);
  }
}

void LittleWindow::Archive(TabId tab, bool session_ending) {
  ArchiveEntry entry;
  entry.url = deps_.tabs->GetLastCommittedUrl(tab);
  entry.title = deps_.tabs->GetTitle(tab);
  entry.closed_at = base::Time::Now();
  entry.reason = "little";
  if (!entry.url.is_valid() || entry.url.is_empty()) {
    return;
  }
  deps_.delegate->Archive(profile_, std::move(entry), session_ending);
}

void LittleWindow::OnWindowClosed(BrowserWindowInterface* browser) {
  // Deletes this.
  LittleWindows().erase(id_);
}

}  // namespace cromarc
