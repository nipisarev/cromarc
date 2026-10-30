// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_registry.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "cromarc/browser/ports/app_lifecycle.h"
#include "cromarc/browser/ports/windows.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// How long a link the user followed may take to open its tab.
constexpr base::TimeDelta kExpectationLifetime = base::Seconds(10);
// How long the close button asks before Esc in a field closes (Zen's 3 s).
constexpr base::TimeDelta kConfirmCloseFor = base::Seconds(3);

}  // namespace

GlanceRegistry::GlanceRegistry(Tabs& tabs,
                               Windows& windows,
                               AppLifecycle& app_lifecycle,
                               GlanceSurface& surface,
                               Delegate& delegate)
    : tabs_(tabs),
      windows_(windows),
      app_lifecycle_(app_lifecycle),
      surface_(surface),
      delegate_(delegate) {
  closing_all_ = app_lifecycle_->OnClosingAllWindows(base::BindRepeating(
      &GlanceRegistry::OnClosingAllWindows, weak_factory_.GetWeakPtr()));
}

GlanceRegistry::~GlanceRegistry() = default;

void GlanceRegistry::Watch(BrowserWindowInterface* window) {
  if (!window || watched_.contains(window)) {
    return;
  }
  Watched& watched = watched_[window];
  watched.inserted = tabs_->OnInserted(
      window, base::BindRepeating(&GlanceRegistry::OnInserted,
                                  weak_factory_.GetWeakPtr(), window));
  watched.active_changed = tabs_->OnActiveChanged(
      window, base::BindRepeating(&GlanceRegistry::OnActiveChanged,
                                  weak_factory_.GetWeakPtr(), window));
  watched.closed = windows_->OnClosed(
      window, base::BindRepeating(
                  [](base::WeakPtr<GlanceRegistry> registry,
                     BrowserWindowInterface* closed) {
                    if (registry) {
                      registry->watched_.erase(closed);
                    }
                  },
                  weak_factory_.GetWeakPtr()));
}

bool GlanceRegistry::Expect(TabId parent, std::optional<gfx::Point> origin) {
  if (IsGlance(parent) || by_parent_.contains(parent)) {
    return false;
  }
  Watch(tabs_->GetWindow(parent));
  expected_[parent] = {/*glance=*/true, origin, base::TimeTicks::Now()};
  // The page shows now; under the glance it will not.
  surface_->Snapshot(parent);
  return true;
}

void GlanceRegistry::ExpectTab(TabId parent) {
  expected_[parent] = {/*glance=*/false, std::nullopt, base::TimeTicks::Now()};
}

void GlanceRegistry::OnNavigationStarting(TabId tab, const GURL& url) {
  auto it = unknown_opened_.find(tab);
  if (it == unknown_opened_.end()) {
    return;
  }
  const TabId opener = it->second;
  unknown_opened_.erase(it);
  BrowserWindowInterface* window = tabs_->GetWindow(tab);
  if (!window || !tabs_->Exists(opener) || by_parent_.contains(opener)) {
    return;
  }
  const OpenedTab opened{delegate_->KindOf(opener), IsGlance(opener),
                         tabs_->GetUrl(opener), url};
  if (OpensAsGlance(opened)) {
    surface_->Snapshot(opener);
    Mark(tab, opener, window, std::nullopt);
  }
}

TabKind GlanceRegistry::KindOf(TabId tab) {
  return delegate_->KindOf(tab);
}

bool GlanceRegistry::IsGlance(TabId tab) const {
  return glances_.contains(tab);
}

std::optional<TabId> GlanceRegistry::GlanceOf(TabId parent) const {
  auto it = by_parent_.find(parent);
  return it == by_parent_.end() ? std::nullopt : std::make_optional(it->second);
}

std::optional<TabId> GlanceRegistry::ParentOf(TabId glance) const {
  auto it = glances_.find(glance);
  return it == glances_.end() ? std::nullopt
                              : std::make_optional(it->second.parent);
}

std::vector<GlanceRegistry::Entry> GlanceRegistry::GlancesIn(
    BrowserWindowInterface* window) const {
  std::vector<Entry> entries;
  for (const auto& [glance, entry] : glances_) {
    if (entry.window == window) {
      entries.push_back({glance, entry.parent});
    }
  }
  return entries;
}

void GlanceRegistry::Close(TabId glance) {
  auto it = glances_.find(glance);
  if (it == glances_.end() || it->second.closing_by_user) {
    return;
  }
  it->second.closing_by_user = true;
  BrowserWindowInterface* window = it->second.window;
  const TabId parent = it->second.parent;
  // Back to the parent, as Zen's closeGlance does, then the tab goes.
  auto close = base::BindOnce(
      [](base::WeakPtr<GlanceRegistry> registry, TabId glance, TabId parent) {
        if (!registry) {
          return;
        }
        if (registry->tabs_->Exists(parent)) {
          registry->tabs_->Activate(parent);
        }
        if (registry->tabs_->Exists(glance)) {
          registry->tabs_->CloseByUser(glance);
        }
      },
      weak_factory_.GetWeakPtr(), glance, parent);
  if (tabs_->GetActive(window) == glance) {
    Hide(window, /*animate=*/true, std::move(close));
  } else {
    std::move(close).Run();
  }
}

void GlanceRegistry::Expand(TabId glance) {
  auto it = glances_.find(glance);
  if (it == glances_.end() || it->second.closing_by_user) {
    return;
  }
  BrowserWindowInterface* window = it->second.window;
  const TabId parent = it->second.parent;
  Unmark(glance);
  Hide(window, /*animate=*/false, base::DoNothing());
  if (delegate_->KindOf(parent) != TabKind::kToday) {
    tabs_->MoveToFirstUnpinned(glance);
  }
  on_changed_.Notify();
}

std::optional<TabId> GlanceRegistry::ActiveGlance(
    BrowserWindowInterface* window) {
  const std::optional<TabId> active = tabs_->GetActive(window);
  return active && IsGlance(*active) ? active : std::nullopt;
}

bool GlanceRegistry::CloseActive(BrowserWindowInterface* window) {
  const std::optional<TabId> glance = ActiveGlance(window);
  if (!glance) {
    return false;
  }
  Close(*glance);
  return true;
}

bool GlanceRegistry::ExpandActive(BrowserWindowInterface* window) {
  const std::optional<TabId> glance = ActiveGlance(window);
  if (!glance) {
    return false;
  }
  Expand(*glance);
  return true;
}

void GlanceRegistry::ActivateTabOrGlance(TabId tab) {
  const std::optional<TabId> glance = GlanceOf(tab);
  tabs_->Activate(glance ? *glance : tab);
}

base::CallbackListSubscription GlanceRegistry::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void GlanceRegistry::CloseGlance(BrowserWindowInterface* window) {
  CloseActive(window);
}

void GlanceRegistry::ExpandGlance(BrowserWindowInterface* window) {
  ExpandActive(window);
}

void GlanceRegistry::EscapeGlance(BrowserWindowInterface* window,
                                  bool in_field) {
  const std::optional<TabId> glance = ActiveGlance(window);
  if (!glance) {
    return;
  }
  Glance& entry = glances_.at(*glance);
  const base::TimeTicks now = base::TimeTicks::Now();
  if (in_field && now >= entry.confirm_until) {
    entry.confirm_until = now + kConfirmCloseFor;
    surface_->AskToConfirmClose(window, kConfirmCloseFor);
    return;
  }
  Close(*glance);
}

void GlanceRegistry::OnInserted(BrowserWindowInterface* window,
                                TabId tab,
                                std::optional<TabId> opener,
                                bool active) {
  // A parent moved to another window leaves its glance behind: it goes to
  // the archive.
  if (const std::optional<TabId> glance = GlanceOf(tab);
      glance && tabs_->GetWindow(*glance) != window) {
    OnParentClosing(tab);
  }
  if (!opener) {
    return;
  }
  // The next tab the parent opens answers the expectation: in front, it is
  // the glance; in the background (Cmd held), an ordinary tab.
  if (auto it = expected_.find(*opener); it != expected_.end()) {
    const Expectation expectation = it->second;
    expected_.erase(it);
    const bool fresh =
        base::TimeTicks::Now() - expectation.since < kExpectationLifetime;
    if (fresh && !expectation.glance) {
      return;
    }
    if (fresh && active) {
      Mark(tab, *opener, window, expectation.origin);
      return;
    }
  }
  if (!active) {
    return;
  }
  const OpenedTab opened{delegate_->KindOf(*opener), IsGlance(*opener),
                         tabs_->GetUrl(*opener), tabs_->GetUrl(tab)};
  if (opened.opener_kind == TabKind::kToday || opened.opener_is_glance ||
      by_parent_.contains(*opener)) {
    return;
  }
  // window.open: the address comes with the tab's first navigation.
  if (!OpensInGlance(opened.url)) {
    unknown_opened_[tab] = *opener;
    return;
  }
  if (OpensAsGlance(opened)) {
    surface_->Snapshot(*opener);
    Mark(tab, *opener, window, std::nullopt);
  }
}

void GlanceRegistry::OnActiveChanged(BrowserWindowInterface* window) {
  const std::optional<TabId> active = tabs_->GetActive(window);
  if (active && IsGlance(*active)) {
    Show(*active);
    return;
  }
  // Its parent came forward: so does the glance (Zen's quickOpenGlance).
  if (active) {
    if (const std::optional<TabId> glance = GlanceOf(*active);
        glance && !glances_.at(*glance).closing_by_user) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&GlanceRegistry::RedirectToGlance,
                                    weak_factory_.GetWeakPtr(), window,
                                    *active));
    }
  }
  // Zen's quickCloseGlance: the glance stays, hidden with its parent.
  Hide(window, /*animate=*/false, base::DoNothing());
}

void GlanceRegistry::RedirectToGlance(BrowserWindowInterface* window,
                                      TabId parent) {
  const std::optional<TabId> glance = GlanceOf(parent);
  if (glance && tabs_->GetActive(window) == parent &&
      !glances_.at(*glance).closing_by_user) {
    tabs_->Activate(*glance);
  }
}

void GlanceRegistry::Mark(TabId tab,
                          TabId parent,
                          BrowserWindowInterface* window,
                          std::optional<gfx::Point> origin) {
  if (by_parent_.contains(parent) || IsGlance(tab)) {
    return;
  }
  Glance& glance = glances_[tab];
  glance.parent = parent;
  glance.window = window;
  glance.origin = origin;
  glance.parent_closes = tabs_->OnWillClose(
      parent, base::BindRepeating(&GlanceRegistry::OnParentClosing,
                                  weak_factory_.GetWeakPtr(), parent));
  glance.closes = tabs_->OnWillClose(
      tab, base::BindRepeating(&GlanceRegistry::OnGlanceClosing,
                               weak_factory_.GetWeakPtr(), tab));
  by_parent_[parent] = tab;
  if (tabs_->GetActive(window) == tab) {
    Show(tab);
  }
  on_changed_.Notify();
}

void GlanceRegistry::Show(TabId tab) {
  Glance& glance = glances_.at(tab);
  auto [it, shown] = shown_.try_emplace(glance.window, tab);
  if (!shown && it->second == tab) {
    return;
  }
  it->second = tab;
  // Shown anew, its close button asks nothing.
  glance.confirm_until = base::TimeTicks();
  surface_->Show(glance.window, tab, glance.parent,
                 std::exchange(glance.origin, std::nullopt), this);
}

void GlanceRegistry::Hide(BrowserWindowInterface* window,
                          bool animate,
                          base::OnceClosure done) {
  if (!shown_.erase(window)) {
    std::move(done).Run();
    return;
  }
  surface_->Hide(window, animate, std::move(done));
}

void GlanceRegistry::Unmark(TabId glance) {
  auto it = glances_.find(glance);
  if (it == glances_.end()) {
    return;
  }
  BrowserWindowInterface* window = it->second.window;
  surface_->Forget(it->second.parent);
  by_parent_.erase(it->second.parent);
  glances_.erase(it);
  if (auto shown = shown_.find(window);
      shown != shown_.end() && shown->second == glance) {
    Hide(window, /*animate=*/false, base::DoNothing());
  }
}

void GlanceRegistry::OnParentClosing(TabId parent) {
  const std::optional<TabId> glance = GlanceOf(parent);
  if (!glance) {
    return;
  }
  // Nothing is lost: the page is in the archive.
  delegate_->Archive(*glance, app_lifecycle_->IsSessionEnding());
  BrowserWindowInterface* window = glances_.at(*glance).window;
  Unmark(*glance);
  // Not while the parent's tab strip is closing it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<GlanceRegistry> registry, TabId tab) {
                       if (registry && registry->tabs_->Exists(tab)) {
                         registry->tabs_->Close(tab);
                       }
                     },
                     weak_factory_.GetWeakPtr(), *glance));
  if (tabs_->GetActive(window) == glance) {
    Hide(window, /*animate=*/false, base::DoNothing());
  }
  on_changed_.Notify();
}

void GlanceRegistry::OnGlanceClosing(TabId glance) {
  auto it = glances_.find(glance);
  if (it == glances_.end()) {
    return;
  }
  // Closed by its window, a quit or the page itself: kept in the archive.
  if (!it->second.closing_by_user) {
    delegate_->Archive(glance, app_lifecycle_->IsSessionEnding());
  }
  Unmark(glance);
  unknown_opened_.erase(glance);
  on_changed_.Notify();
}

void GlanceRegistry::OnClosingAllWindows(bool closing) {
  if (!closing || glances_.empty()) {
    return;
  }
  // A quit does not bring glances back as tabs: they go to the archive, and
  // as tabs closed now the session does not keep them.
  std::vector<TabId> glances;
  for (const auto& [glance, entry] : glances_) {
    glances.push_back(glance);
  }
  for (TabId glance : glances) {
    delegate_->Archive(glance, /*session_ending=*/true);
    Unmark(glance);
  }
  for (TabId glance : glances) {
    if (tabs_->Exists(glance)) {
      tabs_->Close(glance);
    }
  }
  on_changed_.Notify();
}

}  // namespace cromarc
