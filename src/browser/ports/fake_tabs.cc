// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_tabs.h"

#include <algorithm>

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
  active_.try_emplace(window, id);
  return id;
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
  if (auto it = active_changed_.find(t->window); it != active_changed_.end()) {
    it->second->Notify();
  }
}

void FakeTabs::SetPinned(TabId tab, bool pinned) {
  if (Tab* t = Find(tab)) {
    t->pinned = pinned;
  }
}

void FakeTabs::Close(TabId tab) {
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
  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    Activate(tab);
  }
}

void FakeTabs::OpenNewTabPage(BrowserWindowInterface* window) {
  Open(window, GURL("chrome://newtab/"),
       WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

std::optional<TabId> FakeTabs::OpenPinned(BrowserWindowInterface* window,
                                          const GURL& url) {
  opened.emplace_back(window, url);
  const TabId tab = Add(window, /*pinned=*/true);
  Activate(tab);
  return tab;
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
    Activate(tab);
  }
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
