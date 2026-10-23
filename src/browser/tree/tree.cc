// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree.h"

#include <algorithm>

#include "base/check.h"
#include "base/location.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::metrics::BookmarkEditSource;

constexpr char kRootKey[] = "cromarc_root";
constexpr char kCollapsedKey[] = "cromarc_collapsed";
constexpr char kEssentialKey[] = "cromarc_essential";

bool HasFlag(const BookmarkNode* node, const char* key) {
  std::string value;
  return node->GetMetaInfo(key, &value) && value == "1";
}

}  // namespace

Tree::Tree(BookmarkModel* model) : model_(model) {
  observation_.Observe(model_);
}

Tree::~Tree() = default;

// static
std::string Tree::IdOf(const BookmarkNode* node) {
  return node->uuid().AsLowercaseString();
}

// static
bool Tree::IsCollapsed(const BookmarkNode* node) {
  return HasFlag(node, kCollapsedKey);
}

// static
bool Tree::IsEssential(const BookmarkNode* node) {
  return HasFlag(node, kEssentialKey);
}

const BookmarkNode* Tree::FindRoot() const {
  if (!model_->loaded()) {
    return nullptr;
  }
  for (const auto& child : model_->other_node()->children()) {
    if (child->is_folder() && HasFlag(child.get(), kRootKey)) {
      return child.get();
    }
  }
  return nullptr;
}

const BookmarkNode* Tree::Root() {
  CHECK(model_->loaded());
  if (const BookmarkNode* root = FindRoot()) {
    return root;
  }
  const BookmarkNode* other = model_->other_node();
  BookmarkNode::MetaInfoMap meta_info{{kRootKey, "1"}};
  return model_->AddFolder(other, other->children().size(), u"Cromarc",
                           &meta_info);
}

bool Tree::Contains(const BookmarkNode* node) {
  const BookmarkNode* root = FindRoot();
  if (!root) {
    return false;
  }
  for (const BookmarkNode* n = node; n; n = n->parent()) {
    if (n == root) {
      return true;
    }
  }
  return false;
}

const BookmarkNode* Tree::Find(const std::string& id) {
  const base::Uuid uuid = base::Uuid::ParseLowercase(id);
  if (!uuid.is_valid()) {
    return nullptr;
  }
  const BookmarkNode* node = model_->GetNodeByUuid(
      uuid, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  return node && Contains(node) ? node : nullptr;
}

const BookmarkNode* Tree::FindFolder(const std::string& parent_id) {
  if (parent_id.empty()) {
    return Root();
  }
  const BookmarkNode* node = Find(parent_id);
  return node && node->is_folder() ? node : nullptr;
}

const BookmarkNode* Tree::CreateFolder(const std::string& parent_id,
                                       size_t index,
                                       const std::u16string& title) {
  const BookmarkNode* parent = FindFolder(parent_id);
  if (!parent) {
    return nullptr;
  }
  return model_->AddFolder(parent, std::min(index, parent->children().size()),
                           title);
}

const BookmarkNode* Tree::AddPinned(const std::string& parent_id,
                                    size_t index,
                                    const std::u16string& title,
                                    const GURL& home_url) {
  const BookmarkNode* parent = FindFolder(parent_id);
  if (!parent || !home_url.is_valid()) {
    return nullptr;
  }
  return model_->AddURL(parent, std::min(index, parent->children().size()),
                        title, home_url, /*meta_info=*/nullptr,
                        /*creation_time=*/std::nullopt, /*uuid=*/std::nullopt,
                        /*added_by_user=*/true);
}

bool Tree::Move(const std::string& id,
                const std::string& parent_id,
                size_t index) {
  const BookmarkNode* node = Find(id);
  const BookmarkNode* parent = FindFolder(parent_id);
  if (!node || !parent || node == Root()) {
    return false;
  }
  for (const BookmarkNode* n = parent; n; n = n->parent()) {
    if (n == node) {
      return false;
    }
  }
  model_->Move(node, parent, std::min(index, parent->children().size()));
  return true;
}

bool Tree::Rename(const std::string& id, const std::u16string& title) {
  const BookmarkNode* node = Find(id);
  if (!node || node == Root()) {
    return false;
  }
  model_->SetTitle(node, title, BookmarkEditSource::kUser);
  return true;
}

bool Tree::Remove(const std::string& id) {
  const BookmarkNode* node = Find(id);
  if (!node || node == Root()) {
    return false;
  }
  model_->Remove(node, BookmarkEditSource::kUser, FROM_HERE);
  return true;
}

bool Tree::SetHomeUrl(const std::string& id, const GURL& url) {
  const BookmarkNode* node = Find(id);
  if (!node || !node->is_url() || !url.is_valid()) {
    return false;
  }
  model_->SetURL(node, url, BookmarkEditSource::kUser);
  return true;
}

bool Tree::SetCollapsed(const std::string& id, bool collapsed) {
  const BookmarkNode* node = Find(id);
  if (!node || !node->is_folder()) {
    return false;
  }
  SetFlag(node, kCollapsedKey, collapsed);
  return true;
}

bool Tree::SetEssential(const std::string& id, bool essential) {
  const BookmarkNode* node = Find(id);
  if (!node || !node->is_url()) {
    return false;
  }
  if (essential && !IsEssential(node) && EssentialCount() >= kMaxEssentials) {
    return false;
  }
  SetFlag(node, kEssentialKey, essential);
  return true;
}

size_t Tree::EssentialCount() {
  size_t count = 0;
  auto visit = [&count](const BookmarkNode* node, auto& self) -> void {
    if (IsEssential(node)) {
      ++count;
    }
    for (const auto& child : node->children()) {
      self(child.get(), self);
    }
  };
  visit(Root(), visit);
  return count;
}

void Tree::SetFlag(const BookmarkNode* node, const char* key, bool value) {
  if (value) {
    model_->SetNodeMetaInfo(node, key, "1");
  } else {
    model_->DeleteNodeMetaInfo(node, key);
  }
}

base::CallbackListSubscription Tree::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void Tree::NotifyIfInside(const BookmarkNode* node) {
  if (Contains(node)) {
    on_changed_.Notify();
  }
}

void Tree::BookmarkModelLoaded(bool ids_reassigned) {
  on_changed_.Notify();
}

void Tree::BookmarkModelBeingDeleted() {
  observation_.Reset();
}

void Tree::BookmarkNodeMoved(const BookmarkNode* old_parent,
                             size_t old_index,
                             const BookmarkNode* new_parent,
                             size_t new_index) {
  if (Contains(old_parent) || Contains(new_parent)) {
    on_changed_.Notify();
  }
}

void Tree::BookmarkNodeAdded(const BookmarkNode* parent,
                             size_t index,
                             bool added_by_user) {
  NotifyIfInside(parent);
}

void Tree::BookmarkNodeRemoved(const BookmarkNode* parent,
                               size_t old_index,
                               const BookmarkNode* node,
                               const std::set<GURL>& no_longer_bookmarked,
                               const base::Location& location) {
  // Removing the root itself also counts: the next Root() recreates it.
  if (Contains(parent) || HasFlag(node, kRootKey)) {
    on_changed_.Notify();
  }
}

void Tree::BookmarkNodeChanged(const BookmarkNode* node) {
  NotifyIfInside(node);
}

void Tree::BookmarkMetaInfoChanged(const BookmarkNode* node) {
  NotifyIfInside(node);
}

void Tree::BookmarkNodeFaviconChanged(const BookmarkNode* node) {}

void Tree::BookmarkNodeChildrenReordered(const BookmarkNode* node) {
  NotifyIfInside(node);
}

void Tree::BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                       const base::Location& location) {
  on_changed_.Notify();
}

}  // namespace cromarc
