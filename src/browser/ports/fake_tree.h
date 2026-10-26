// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_TREE_H_
#define CROMARC_BROWSER_PORTS_FAKE_TREE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/tree.h"

namespace cromarc {

// A tree in memory that keeps Tree's contract (bookmark_tree_unittest.cc runs
// the contract's tests on it and on BookmarkTree).
class FakeTree : public Tree {
 public:
  FakeTree();
  ~FakeTree() override;

  // Not loaded until FinishLoading() when false.
  explicit FakeTree(bool loaded);
  void FinishLoading();

  // Tree:
  bool IsLoaded() const override;
  Node Root() override;
  std::optional<Node> Find(const std::string& id) override;
  std::optional<std::string> CreateFolder(const std::string& parent_id,
                                          size_t index,
                                          const std::u16string& title) override;
  std::optional<std::string> AddPinned(const std::string& parent_id,
                                       size_t index,
                                       const std::u16string& title,
                                       const GURL& home_url) override;
  bool Move(const std::string& id,
            const std::string& parent_id,
            size_t index) override;
  bool Rename(const std::string& id, const std::u16string& title) override;
  bool Remove(const std::string& id) override;
  bool SetHomeUrl(const std::string& id, const GURL& url) override;
  bool SetCollapsed(const std::string& id, bool collapsed) override;
  bool SetEssential(const std::string& id, bool essential) override;
  size_t EssentialCount() override;
  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback) override;

 private:
  struct Item {
    Item();
    ~Item();

    std::string id;
    std::u16string title;
    bool is_folder = false;
    bool collapsed = false;
    bool essential = false;
    GURL home_url;
    raw_ptr<Item> parent = nullptr;
    std::vector<std::unique_ptr<Item>> children;
  };

  Item* FindItem(const std::string& id);
  Item* FindFolder(const std::string& parent_id);
  Item* Insert(Item* parent, size_t index, std::unique_ptr<Item> item);
  static Node NodeOf(const Item& item);
  void Changed();

  bool loaded_ = true;
  int next_id_ = 1;
  std::unique_ptr<Item> root_;
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_TREE_H_
