// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_BOOKMARK_TREE_H_
#define CROMARC_BROWSER_CHROMIUM_BOOKMARK_TREE_H_

#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "cromarc/browser/ports/tree.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace cromarc {

// Tree in Chromium bookmarks, under a "Cromarc" folder in Other bookmarks, so
// that sync carries it like any bookmark. A folder of the tree is a bookmark
// folder; a pinned item is a URL bookmark whose URL is its home URL; an
// item's id is its bookmark's UUID. Cromarc's own fields live in the
// bookmark's meta info.
class BookmarkTree : public Tree, public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkTree(bookmarks::BookmarkModel* model);
  BookmarkTree(const BookmarkTree&) = delete;
  BookmarkTree& operator=(const BookmarkTree&) = delete;
  ~BookmarkTree() override;

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
  // The "Cromarc" folder, created when missing.
  const bookmarks::BookmarkNode* RootFolder();
  // The "Cromarc" folder if it exists; never creates it (safe in observers).
  const bookmarks::BookmarkNode* FindRoot() const;
  bool Contains(const bookmarks::BookmarkNode* node) const;
  // Null for ids outside the tree.
  const bookmarks::BookmarkNode* FindNode(const std::string& id);
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

#endif  // CROMARC_BROWSER_CHROMIUM_BOOKMARK_TREE_H_
