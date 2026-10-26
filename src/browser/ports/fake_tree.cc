// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_tree.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"

namespace cromarc {

FakeTree::Item::Item() = default;
FakeTree::Item::~Item() = default;

FakeTree::FakeTree() : FakeTree(/*loaded=*/true) {}

FakeTree::FakeTree(bool loaded)
    : loaded_(loaded), root_(std::make_unique<Item>()) {
  root_->id = "root";
  root_->title = u"Cromarc";
  root_->is_folder = true;
}

FakeTree::~FakeTree() = default;

void FakeTree::FinishLoading() {
  loaded_ = true;
  Changed();
}

bool FakeTree::IsLoaded() const {
  return loaded_;
}

Tree::Node FakeTree::NodeOf(const Item& item) {
  Node node;
  node.id = item.id;
  node.title = item.title;
  node.is_folder = item.is_folder;
  node.collapsed = item.collapsed;
  node.home_url = item.home_url;
  node.essential = item.essential;
  for (const auto& child : item.children) {
    node.children.push_back(NodeOf(*child));
  }
  return node;
}

FakeTree::Item* FakeTree::FindItem(const std::string& id) {
  auto visit = [&id](Item* item, auto& self) -> Item* {
    if (item->id == id) {
      return item;
    }
    for (const auto& child : item->children) {
      if (Item* found = self(child.get(), self)) {
        return found;
      }
    }
    return nullptr;
  };
  return visit(root_.get(), visit);
}

FakeTree::Item* FakeTree::FindFolder(const std::string& parent_id) {
  Item* item = parent_id.empty() ? root_.get() : FindItem(parent_id);
  return item && item->is_folder ? item : nullptr;
}

FakeTree::Item* FakeTree::Insert(Item* parent,
                                 size_t index,
                                 std::unique_ptr<Item> item) {
  item->parent = parent;
  Item* inserted = item.get();
  parent->children.insert(
      parent->children.begin() +
          static_cast<std::ptrdiff_t>(std::min(index, parent->children.size())),
      std::move(item));
  Changed();
  return inserted;
}

Tree::Node FakeTree::Root() {
  return NodeOf(*root_);
}

std::optional<Tree::Node> FakeTree::Find(const std::string& id) {
  Item* item = FindItem(id);
  if (!item) {
    return std::nullopt;
  }
  return NodeOf(*item);
}

std::optional<std::string> FakeTree::CreateFolder(
    const std::string& parent_id,
    size_t index,
    const std::u16string& title) {
  Item* parent = FindFolder(parent_id);
  if (!parent) {
    return std::nullopt;
  }
  auto item = std::make_unique<Item>();
  item->id = "item-" + base::NumberToString(next_id_++);
  item->title = title;
  item->is_folder = true;
  return Insert(parent, index, std::move(item))->id;
}

std::optional<std::string> FakeTree::AddPinned(const std::string& parent_id,
                                               size_t index,
                                               const std::u16string& title,
                                               const GURL& home_url) {
  Item* parent = FindFolder(parent_id);
  if (!parent || !home_url.is_valid()) {
    return std::nullopt;
  }
  auto item = std::make_unique<Item>();
  item->id = "item-" + base::NumberToString(next_id_++);
  item->title = title;
  item->home_url = home_url;
  return Insert(parent, index, std::move(item))->id;
}

bool FakeTree::Move(const std::string& id,
                    const std::string& parent_id,
                    size_t index) {
  Item* item = FindItem(id);
  Item* parent = FindFolder(parent_id);
  if (!item || !parent || item == root_.get()) {
    return false;
  }
  for (Item* ancestor = parent; ancestor; ancestor = ancestor->parent) {
    if (ancestor == item) {
      return false;
    }
  }
  index = std::min(index, parent->children.size());
  std::vector<std::unique_ptr<Item>>& siblings = item->parent->children;
  auto it = std::ranges::find(siblings, item, &std::unique_ptr<Item>::get);
  // As in BookmarkModel::Move, the index counts the item where it is now.
  const size_t old_index = static_cast<size_t>(it - siblings.begin());
  if (item->parent == parent) {
    if (index == old_index || index == old_index + 1) {
      return true;
    }
    if (index > old_index) {
      --index;
    }
  }
  std::unique_ptr<Item> moved = std::move(*it);
  siblings.erase(it);
  Insert(parent, index, std::move(moved));
  return true;
}

bool FakeTree::Rename(const std::string& id, const std::u16string& title) {
  Item* item = FindItem(id);
  if (!item || item == root_.get()) {
    return false;
  }
  if (item->title != title) {
    item->title = title;
    Changed();
  }
  return true;
}

bool FakeTree::Remove(const std::string& id) {
  Item* item = FindItem(id);
  if (!item || item == root_.get()) {
    return false;
  }
  std::erase_if(item->parent->children,
                [item](const auto& child) { return child.get() == item; });
  Changed();
  return true;
}

bool FakeTree::SetHomeUrl(const std::string& id, const GURL& url) {
  Item* item = FindItem(id);
  if (!item || item->is_folder || !url.is_valid()) {
    return false;
  }
  if (item->home_url != url) {
    item->home_url = url;
    Changed();
  }
  return true;
}

bool FakeTree::SetCollapsed(const std::string& id, bool collapsed) {
  Item* item = FindItem(id);
  if (!item || !item->is_folder) {
    return false;
  }
  if (item->collapsed != collapsed) {
    item->collapsed = collapsed;
    Changed();
  }
  return true;
}

bool FakeTree::SetEssential(const std::string& id, bool essential) {
  Item* item = FindItem(id);
  if (!item || item->is_folder) {
    return false;
  }
  if (essential && !item->essential && EssentialCount() >= kMaxEssentials) {
    return false;
  }
  if (item->essential != essential) {
    item->essential = essential;
    Changed();
  }
  return true;
}

size_t FakeTree::EssentialCount() {
  size_t count = 0;
  auto visit = [&count](const Item& item, auto& self) -> void {
    if (item.essential) {
      ++count;
    }
    for (const auto& child : item.children) {
      self(*child, self);
    }
  };
  visit(*root_, visit);
  return count;
}

base::CallbackListSubscription FakeTree::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void FakeTree::Changed() {
  on_changed_.Notify();
}

}  // namespace cromarc
