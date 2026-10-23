// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_TREE_TREE_H_
#define CROMARC_BROWSER_TREE_TREE_H_

#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace cromarc {

// The Arc-style tree of one space (= profile): folders nested to any depth and
// pinned items, kept in Chromium bookmarks under a "Cromarc" folder in Other
// bookmarks, so that sync carries it like any bookmark. A folder of the tree is
// a bookmark folder; a pinned item is a URL bookmark whose URL is its home URL.
// Cromarc's own fields live in the bookmark's meta info.
//
// Items are addressed by bookmark UUID; "" means the tree root. Every method
// needs a loaded BookmarkModel.
class Tree : public bookmarks::BookmarkModelObserver {
 public:
  static constexpr size_t kMaxEssentials = 12;

  explicit Tree(bookmarks::BookmarkModel* model);
  Tree(const Tree&) = delete;
  Tree& operator=(const Tree&) = delete;
  ~Tree() override;

  static std::string IdOf(const bookmarks::BookmarkNode* node);
  static bool IsCollapsed(const bookmarks::BookmarkNode* node);
  static bool IsEssential(const bookmarks::BookmarkNode* node);

  // The "Cromarc" folder, created when missing.
  const bookmarks::BookmarkNode* Root();
  // Null for ids outside the tree.
  const bookmarks::BookmarkNode* Find(const std::string& id);

  const bookmarks::BookmarkNode* CreateFolder(const std::string& parent_id,
                                              size_t index,
                                              const std::u16string& title);
  const bookmarks::BookmarkNode* AddPinned(const std::string& parent_id,
                                           size_t index,
                                           const std::u16string& title,
                                           const GURL& home_url);
  bool Move(const std::string& id, const std::string& parent_id, size_t index);
  bool Rename(const std::string& id, const std::u16string& title);
  bool Remove(const std::string& id);
  bool SetHomeUrl(const std::string& id, const GURL& url);
  bool SetCollapsed(const std::string& id, bool collapsed);
  bool SetEssential(const std::string& id, bool essential);
  size_t EssentialCount();

  bool Contains(const bookmarks::BookmarkNode* node);

  // Runs after every change inside the tree, and once the model loads.
  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback);

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

 private:
  // The "Cromarc" folder if it exists; never creates it (safe in observers).
  const bookmarks::BookmarkNode* FindRoot() const;
  // Folder for `parent_id`: the root for "", a tree folder, or null.
  const bookmarks::BookmarkNode* FindFolder(const std::string& parent_id);
  void NotifyIfInside(const bookmarks::BookmarkNode* node);
  void SetFlag(const bookmarks::BookmarkNode* node,
               const char* key,
               bool value);

  raw_ptr<bookmarks::BookmarkModel> model_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      observation_{this};
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_TREE_TREE_H_
