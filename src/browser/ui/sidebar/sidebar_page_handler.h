// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BrowserWindowInterface;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace cromarc {

class TreeService;

// Tree operations for the sidebar of `browser`.
class SidebarPageHandler : public mojom::PageHandler {
 public:
  SidebarPageHandler(mojo::PendingReceiver<mojom::PageHandler> receiver,
                     mojo::PendingRemote<mojom::Page> page,
                     BrowserWindowInterface* browser,
                     TreeService* tree_service);
  SidebarPageHandler(const SidebarPageHandler&) = delete;
  SidebarPageHandler& operator=(const SidebarPageHandler&) = delete;
  ~SidebarPageHandler() override;

  // mojom::PageHandler:
  void GetTree(GetTreeCallback callback) override;
  void OpenItem(const std::string& id) override;
  void CreateFolder(const std::string& parent_id,
                    uint32_t index,
                    const std::string& title,
                    CreateFolderCallback callback) override;
  void PinTab(const tabs_api::NodeId& tab,
              const std::string& parent_id,
              uint32_t index,
              bool essential) override;
  void MoveItem(const std::string& id,
                const std::string& parent_id,
                uint32_t index) override;
  void RenameItem(const std::string& id, const std::string& title) override;
  void RemoveItem(const std::string& id) override;
  void ResetItem(const std::string& id) override;
  void SetHomeToCurrent(const std::string& id) override;
  void SetCollapsed(const std::string& id, bool collapsed) override;
  void SetEssential(const std::string& id, bool essential) override;
  void NewTab() override;

 private:
  std::vector<mojom::TreeNodePtr> BuildChildren(
      const bookmarks::BookmarkNode* folder);
  // Returns pinned items' live tabs inside `node` (itself included) to Today.
  void ReleaseLiveTabs(const bookmarks::BookmarkNode* node);
  void OnTreeChanged();

  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TreeService> tree_service_;
  base::CallbackListSubscription tree_subscription_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_
