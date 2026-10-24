// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "cromarc/browser/tree/tree_service_factory.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace cromarc {

// static
TreeService* TreeService::Get(Profile* profile) {
  return TreeServiceFactory::GetForProfile(profile);
}

TreeService::TreeService(bookmarks::BookmarkModel* model)
    : tree_(model),
      tree_subscription_(tree_.RegisterOnChanged(base::BindRepeating(
          &TreeService::NotifyChanged, base::Unretained(this)))),
      model_(model) {}

TreeService::~TreeService() = default;

bool TreeService::loaded() const {
  return model_->loaded();
}

tabs::TabInterface* TreeService::GetLiveTab(const std::string& item_id) {
  auto it = links_.find(item_id);
  if (it == links_.end()) {
    return nullptr;
  }
  tabs::TabInterface* tab = it->second.Get();
  if (!tab) {
    links_.erase(it);
  }
  return tab;
}

std::optional<std::string> TreeService::GetItemForTab(
    const tabs::TabInterface* tab) {
  for (const auto& [item_id, handle] : links_) {
    if (handle.Get() == tab) {
      return item_id;
    }
  }
  return std::nullopt;
}

void TreeService::Link(const std::string& item_id, tabs::TabInterface* tab) {
  links_[item_id] = tab->GetHandle();
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
  if (tabs::TabInterface* tab = GetLiveTab(item_id)) {
    BrowserWindowInterface* owner = tab->GetBrowserWindowInterface();
    TabStripModel* model = owner->GetTabStripModel();
    model->ActivateTabAt(model->GetIndexOfTab(tab));
    if (owner != browser) {
      owner->GetWindow()->Activate();
    }
    return;
  }
  // Asleep: its home URL, as a new pinned tab of `browser`.
  browser->OpenGURL(node->url(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  TabStripModel* model = browser->GetTabStripModel();
  tabs::TabInterface* tab = model->GetTabAtIndex(model->active_index());
  if (!tab) {
    return;
  }
  const int index = model->GetIndexOfTab(tab);
  if (!model->IsTabPinned(index)) {
    model->SetTabPinned(index, true);
  }
  Link(item_id, tab);
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

void TreeService::NotifyChanged() {
  on_changed_.Notify();
}

}  // namespace cromarc
