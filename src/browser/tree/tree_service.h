// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_TREE_TREE_SERVICE_H_
#define CROMARC_BROWSER_TREE_TREE_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/tree.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace cromarc {

class Windows;

// One per regular profile (= space): the tree of folders and pinned items, and
// which live tab belongs to which pinned item. A pinned item without a live tab
// is asleep; opening it loads its home URL. Links are not persisted: on
// startup every pinned item is asleep (design, component 3).
class TreeService : public KeyedService {
 public:
  // Null for profiles without a tree (incognito, guest, system).
  static TreeService* Get(Profile* profile);

  TreeService(std::unique_ptr<Tree> tree, Tabs& tabs, Windows& windows);
  TreeService(const TreeService&) = delete;
  TreeService& operator=(const TreeService&) = delete;
  ~TreeService() override;

  bool loaded() const;
  Tree& tree() { return *tree_; }

  // The live tab of a pinned item; nullopt when it sleeps.
  std::optional<TabId> GetLiveTab(const std::string& item_id);
  // The pinned item `tab` belongs to, if any.
  std::optional<std::string> GetItemForTab(TabId tab);
  void Link(const std::string& item_id, TabId tab);
  void Unlink(const std::string& item_id);

  // Focuses the live tab of a pinned item, or opens its home URL in `browser`
  // as a pinned tab linked to the item.
  void Open(BrowserWindowInterface* browser, const std::string& item_id);
  // The pinned item whose home URL is `url` (ignoring the fragment and a
  // trailing slash), if any.
  std::optional<std::string> FindItemByHomeUrl(const GURL& url);

  // Runs when the tree or a link changes.
  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback);
  // Runs `changes`; observers hear of them once, after: a sidebar never draws
  // the steps in between (an item added before its tab is linked).
  void Batch(base::FunctionRef<void()> changes);

 private:
  void NotifyChanged();

  const std::unique_ptr<Tree> tree_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
  std::map<std::string, TabId> links_;
  base::RepeatingClosureList on_changed_;
  bool batching_ = false;
  bool changed_in_batch_ = false;
  base::CallbackListSubscription tree_subscription_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_TREE_TREE_SERVICE_H_
