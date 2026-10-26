// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_tabs.h"

#include <algorithm>

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
  std::erase_if(tabs_, [tab](const Tab& t) { return t.id == tab; });
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
  if (Tab* t = Find(tab)) {
    active_[t->window] = tab;
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
                    bool background) {
  opened.emplace_back(window, url);
  const TabId tab = Add(window);
  if (!background) {
    Activate(tab);
  }
}

void FakeTabs::OpenNewTabPage(BrowserWindowInterface* window) {
  Open(window, GURL("chrome://newtab/"), /*background=*/false);
}

std::optional<TabId> FakeTabs::OpenPinned(BrowserWindowInterface* window,
                                          const GURL& url) {
  opened.emplace_back(window, url);
  const TabId tab = Add(window, /*pinned=*/true);
  Activate(tab);
  return tab;
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

}  // namespace cromarc
