// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_TREE_H_
#define CROMARC_BROWSER_PORTS_TREE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace cromarc {

// The Arc-style tree of one space (= profile): folders nested to any depth
// and pinned items, which sync carries. Items are addressed by id; "" means
// the tree's root folder. Every method but IsLoaded() needs a loaded tree.
class Tree {
 public:
  static constexpr size_t kMaxEssentials = 12;

  // An item as it is now, with what is under it.
  struct Node {
    Node();
    Node(const Node&);
    Node(Node&&);
    Node& operator=(const Node&);
    Node& operator=(Node&&);
    ~Node();

    std::string id;
    std::u16string title;
    bool is_folder = false;
    // A folder's.
    bool collapsed = false;
    std::vector<Node> children;
    // A pinned item's.
    GURL home_url;
    bool essential = false;
  };

  virtual ~Tree() = default;

  virtual bool IsLoaded() const = 0;
  // The root folder, made when missing, with everything in it.
  virtual Node Root() = 0;
  // Nullopt for ids outside the tree.
  virtual std::optional<Node> Find(const std::string& id) = 0;

  // The new item's id; nullopt for an unknown parent (or an invalid home
  // URL). `index` past the end is the end.
  virtual std::optional<std::string> CreateFolder(
      const std::string& parent_id,
      size_t index,
      const std::u16string& title) = 0;
  virtual std::optional<std::string> AddPinned(const std::string& parent_id,
                                               size_t index,
                                               const std::u16string& title,
                                               const GURL& home_url) = 0;
  // False for the root, unknown ids and a folder into itself.
  virtual bool Move(const std::string& id,
                    const std::string& parent_id,
                    size_t index) = 0;
  virtual bool Rename(const std::string& id, const std::u16string& title) = 0;
  // With what is under it; false for the root.
  virtual bool Remove(const std::string& id) = 0;
  // Pinned items only.
  virtual bool SetHomeUrl(const std::string& id, const GURL& url) = 0;
  // Folders only.
  virtual bool SetCollapsed(const std::string& id, bool collapsed) = 0;
  // Pinned items only, at most kMaxEssentials.
  virtual bool SetEssential(const std::string& id, bool essential) = 0;
  virtual size_t EssentialCount() = 0;

  // Runs after every change inside the tree, and once it loads.
  virtual base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_TREE_H_
