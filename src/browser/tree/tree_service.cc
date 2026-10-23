// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "cromarc/browser/tree/tree_service_factory.h"

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

base::CallbackListSubscription TreeService::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void TreeService::NotifyChanged() {
  on_changed_.Notify();
}

}  // namespace cromarc
