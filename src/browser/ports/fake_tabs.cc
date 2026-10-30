// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_tabs.h"

#include <algorithm>

#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"

namespace cromarc {

FakeTabs::FakeTabs() = default;
FakeTabs::~FakeTabs() = default;

FakeTabs::Tab* FakeTabs::Find(TabId tab) {
  auto it = std::ranges::find(tabs_, tab, &Tab::id);
  return it == tabs_.end() ? nullptr : &*it;
}

TabId FakeTabs::Add(BrowserWindowInterface* window, bool pinned) {
  const TabId id(next_id_++);
  tabs_.push_back({id, window, pinned});
  const bool active = active_.try_emplace(window, id).second;
  NotifyInserted(id, std::nullopt, active);
  if (active) {
    NotifyActiveChanged(window);
  }
  return id;
}

TabId FakeTabs::AddFrom(BrowserWindowInterface* window,
                        std::optional<TabId> opener,
                        const GURL& url,
                        bool active) {
  const TabId id(next_id_++);
  tabs_.push_back({id, window, false, url});
  const bool first = active_.try_emplace(window, id).second;
  if (active && !first) {
    active_[window] = id;
  }
  // Chrome activates an inserted tab before observers hear of the insertion.
  NotifyInserted(id, opener, active || first);
  if (active || first) {
    NotifyActiveChanged(window);
  }
  return id;
}

void FakeTabs::NotifyActiveChanged(BrowserWindowInterface* window) {
  if (auto it = active_changed_.find(window); it != active_changed_.end()) {
    it->second->Notify();
  }
}

void FakeTabs::SetUrl(TabId tab, const GURL& url) {
  if (Tab* t = Find(tab)) {
    t->url = url;
  }
}

void FakeTabs::NotifyInserted(TabId tab,
                              std::optional<TabId> opener,
                              bool active) {
  BrowserWindowInterface* window = GetWindow(tab);
  if (auto it = inserted_.find(window); it != inserted_.end()) {
    it->second->Notify(tab, opener, active);
  }
}

void FakeTabs::Remove(TabId tab) {
  BrowserWindowInterface* window = GetWindow(tab);
  const bool was_active = window && GetActive(window) == tab;
  if (auto it = will_close_.find(tab); it != will_close_.end()) {
    it->second->Notify();
  }
  std::erase_if(tabs_, [tab](const Tab& t) { return t.id == tab; });
  if (was_active) {
    active_.erase(window);
    if (!List(window).empty()) {
      Activate(List(window).back());
    }
  }
}

void FakeTabs::Detach(TabId tab) {
  if (Tab* t = Find(tab)) {
    t->window = nullptr;
  }
}

void FakeTabs::OpenInBackground(BrowserWindowInterface* window) {
  if (auto it = background_.find(window); it != background_.end()) {
    it->second->Notify();
  }
}

bool FakeTabs::Exists(TabId tab) {
  return Find(tab);
}

content::WebContents* FakeTabs::GetContents(TabId tab) {
  return nullptr;
}

BrowserWindowInterface* FakeTabs::GetWindow(TabId tab) {
  Tab* t = Find(tab);
  return t ? t->window : nullptr;
}

std::optional<TabId> FakeTabs::FromContents(content::WebContents* contents) {
  return std::nullopt;
}

GURL FakeTabs::GetUrl(TabId tab) {
  Tab* t = Find(tab);
  return t ? t->url : GURL();
}

GURL FakeTabs::GetLastCommittedUrl(TabId tab) {
  return GetUrl(tab);
}

std::u16string FakeTabs::GetTitle(TabId tab) {
  auto it = titles.find(tab);
  return it == titles.end() ? std::u16string() : it->second;
}

base::Time FakeTabs::GetLastActiveTime(TabId tab) {
  Tab* t = Find(tab);
  return t ? t->last_active : base::Time();
}

bool FakeTabs::IsAudibleOrCaptured(TabId tab) {
  return audible.contains(tab);
}

SiteSecurity FakeTabs::GetSiteSecurity(TabId tab) {
  const auto it = security.find(tab);
  return it == security.end() ? SiteSecurity::kNone : it->second;
}

bool FakeTabs::MayLoseInput(TabId tab) {
  return may_lose_input.contains(tab);
}

std::vector<TabId> FakeTabs::List(BrowserWindowInterface* window) {
  std::vector<TabId> list;
  for (const Tab& tab : tabs_) {
    if (tab.window == window) {
      list.push_back(tab.id);
    }
  }
  return list;
}

std::optional<TabId> FakeTabs::GetActive(BrowserWindowInterface* window) {
  auto it = active_.find(window);
  if (it == active_.end() || GetWindow(it->second) != window) {
    return std::nullopt;
  }
  return it->second;
}

bool FakeTabs::IsPinned(TabId tab) {
  Tab* t = Find(tab);
  return t && t->pinned;
}

void FakeTabs::Activate(TabId tab) {
  Tab* t = Find(tab);
  if (!t || active_[t->window] == tab) {
    return;
  }
  active_[t->window] = tab;
  t->last_active = base::Time::Now();
  NotifyActiveChanged(t->window);
}

void FakeTabs::SetPinned(TabId tab, bool pinned) {
  Tab* t = Find(tab);
  if (!t || t->pinned == pinned) {
    return;
  }
  t->pinned = pinned;
  // As Chrome's strip: pinned tabs come first.
  MoveToFirstUnpinned(tab);
}

void FakeTabs::Close(TabId tab) {
  closed.push_back(tab);
  Remove(tab);
}

void FakeTabs::CloseByUser(TabId tab) {
  closed.push_back(tab);
  Remove(tab);
}

std::optional<TabId> FakeTabs::Duplicate(TabId tab) {
  Tab* t = Find(tab);
  if (!t) {
    return std::nullopt;
  }
  return Add(t->window, t->pinned);
}

void FakeTabs::Open(BrowserWindowInterface* window,
                    const GURL& url,
                    WindowOpenDisposition disposition) {
  opened.emplace_back(window, url);
  const TabId tab = Add(window);
  SetUrl(tab, url);
  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    Activate(tab);
  }
}

void FakeTabs::OpenNewTabPage(BrowserWindowInterface* window) {
  Open(window, GURL("chrome://newtab/"),
       WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void FakeTabs::AddNewTabPage(BrowserWindowInterface* window,
                             bool keep_window_hidden) {
  OpenNewTabPage(window);
}

std::optional<TabId> FakeTabs::OpenPinned(BrowserWindowInterface* window,
                                          const GURL& url) {
  opened.emplace_back(window, url);
  const TabId tab = Add(window, /*pinned=*/true);
  Activate(tab);
  return tab;
}

void FakeTabs::AddUnloaded(BrowserWindowInterface* window,
                           const GURL& url,
                           const std::u16string& title) {
  opened.emplace_back(window, url);
  Add(window);
}

std::optional<TabId> FakeTabs::InsertAfterActive(
    BrowserWindowInterface* window,
    std::unique_ptr<content::WebContents> contents) {
  const TabId tab = Add(window);
  Activate(tab);
  return tab;
}

content::WebContents* FakeTabs::AddNewContents(
    BrowserWindowInterface* window,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features) {
  Open(window, target_url, disposition);
  return nullptr;
}

void FakeTabs::MoveToWindow(TabId tab, BrowserWindowInterface* window) {
  if (Tab* t = Find(tab)) {
    t->window = window;
    active_[window] = tab;
    NotifyInserted(tab, std::nullopt, /*active=*/true);
    NotifyActiveChanged(window);
  }
}

void FakeTabs::OpenFrom(TabId tab, const content::OpenURLParams& params) {
  opened_from.emplace_back(tab, params.url);
  if (BrowserWindowInterface* window = GetWindow(tab)) {
    AddFrom(window, tab, params.url,
            params.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

void FakeTabs::MoveToFirstUnpinned(TabId tab) {
  auto it = std::ranges::find(tabs_, tab, &Tab::id);
  if (it == tabs_.end()) {
    return;
  }
  // Where the window's pinned tabs end.
  Tab moved = *it;
  tabs_.erase(it);
  auto to = std::ranges::find_if(tabs_, [&moved](const Tab& t) {
    return t.window == moved.window && !t.pinned;
  });
  tabs_.insert(to, moved);
}

base::CallbackListSubscription FakeTabs::OnInserted(
    BrowserWindowInterface* window,
    InsertedCallback callback) {
  auto& list = inserted_[window];
  if (!list) {
    list = std::make_unique<
        base::RepeatingCallbackList<void(TabId, std::optional<TabId>, bool)>>();
  }
  return list->Add(std::move(callback));
}

base::CallbackListSubscription FakeTabs::OnOpenedInBackground(
    BrowserWindowInterface* window,
    base::RepeatingClosure callback) {
  std::unique_ptr<base::RepeatingClosureList>& list = background_[window];
  if (!list) {
    list = std::make_unique<base::RepeatingClosureList>();
  }
  return list->Add(std::move(callback));
}

base::CallbackListSubscription FakeTabs::OnActiveChanged(
    BrowserWindowInterface* window,
    base::RepeatingClosure callback) {
  std::unique_ptr<base::RepeatingClosureList>& list = active_changed_[window];
  if (!list) {
    list = std::make_unique<base::RepeatingClosureList>();
  }
  return list->Add(std::move(callback));
}

base::CallbackListSubscription FakeTabs::OnWillClose(
    TabId tab,
    base::RepeatingClosure callback) {
  std::unique_ptr<base::RepeatingClosureList>& list = will_close_[tab];
  if (!list) {
    list = std::make_unique<base::RepeatingClosureList>();
  }
  return list->Add(std::move(callback));
}

}  // namespace cromarc
