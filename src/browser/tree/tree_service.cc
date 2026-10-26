// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree_service.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "cromarc/browser/ports/windows.h"
#include "url/gurl.h"

namespace cromarc {

TreeService::TreeService(bookmarks::BookmarkModel* model,
                         Tabs& tabs,
                         Windows& windows)
    : tree_(model),
      tabs_(tabs),
      windows_(windows),
      tree_subscription_(tree_.RegisterOnChanged(base::BindRepeating(
          &TreeService::NotifyChanged, base::Unretained(this)))),
      model_(model) {}

TreeService::~TreeService() = default;

bool TreeService::loaded() const {
  return model_->loaded();
}

std::optional<TabId> TreeService::GetLiveTab(const std::string& item_id) {
  auto it = links_.find(item_id);
  if (it == links_.end()) {
    return std::nullopt;
  }
  if (!tabs_->Exists(it->second)) {
    links_.erase(it);
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string> TreeService::GetItemForTab(TabId tab) {
  for (const auto& [item_id, linked] : links_) {
    if (linked == tab) {
      return item_id;
    }
  }
  return std::nullopt;
}

void TreeService::Link(const std::string& item_id, TabId tab) {
  links_[item_id] = tab;
  NotifyChanged();
}

void TreeService::Unlink(const std::string& item_id) {
  if (links_.erase(item_id)) {
    NotifyChanged();
  }
}

namespace {

GURL WithoutFragmentOrTrailingSlash(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  GURL stripped = url.ReplaceComponents(replacements);
  std::string spec = stripped.spec();
  if (spec.ends_with('/')) {
    spec.pop_back();
  }
  return GURL(spec);
}

}  // namespace

void TreeService::Open(BrowserWindowInterface* browser,
                       const std::string& item_id) {
  const bookmarks::BookmarkNode* node = tree_.Find(item_id);
  if (!node || !node->is_url()) {
    return;
  }
  if (std::optional<TabId> tab = GetLiveTab(item_id)) {
    BrowserWindowInterface* owner = tabs_->GetWindow(*tab);
    tabs_->Activate(*tab);
    if (owner != browser) {
      windows_->Activate(owner);
    }
    return;
  }
  // Asleep: its home URL, as a new tab of `browser` pinned from the start.
  // A sidebar holds a pinned tab no item claims yet out of Today, so it wakes
  // in its place, as Zen's unloaded pinned tab loads where it is.
  if (std::optional<TabId> tab = tabs_->OpenPinned(browser, node->url())) {
    Link(item_id, *tab);
  }
}

std::optional<std::string> TreeService::FindItemByHomeUrl(const GURL& url) {
  const GURL wanted = WithoutFragmentOrTrailingSlash(url);
  std::optional<std::string> found;
  auto visit = [&](const bookmarks::BookmarkNode* node, auto& self) -> void {
    for (const auto& child : node->children()) {
      if (found) {
        return;
      }
      if (child->is_folder()) {
        self(child.get(), self);
      } else if (WithoutFragmentOrTrailingSlash(child->url()) == wanted) {
        found = Tree::IdOf(child.get());
      }
    }
  };
  if (loaded()) {
    visit(tree_.Root(), visit);
  }
  return found;
}

base::CallbackListSubscription TreeService::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void TreeService::Batch(base::FunctionRef<void()> changes) {
  const bool outermost = !batching_;
  {
    base::AutoReset<bool> batching(&batching_, true);
    changes();
  }
  if (outermost && std::exchange(changed_in_batch_, false)) {
    on_changed_.Notify();
  }
}

void TreeService::NotifyChanged() {
  if (batching_) {
    changed_in_batch_ = true;
    return;
  }
  on_changed_.Notify();
}

}  // namespace cromarc
