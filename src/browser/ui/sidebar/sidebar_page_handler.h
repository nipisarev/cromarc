// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/sidebar/library_backend.h"
#include "cromarc/browser/ports/library.h"
#include "cromarc/browser/ports/tree.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace cromarc {

class AppLifecycle;
class ArchiveService;
class Favicons;
class LifecycleService;
class Profiles;
class Shell;
class TabStripApi;
class Tabs;
class TreeService;
class Windows;

// Tree operations for the sidebar of `browser`.
class SidebarPageHandler : public mojom::PageHandler,
                           public SpaceController::Sidebar {
 public:
  SidebarPageHandler(mojo::PendingReceiver<mojom::PageHandler> receiver,
                     mojo::PendingRemote<mojom::Page> page,
                     BrowserWindowInterface* browser,
                     TreeService* tree_service,
                     LifecycleService* lifecycle_service,
                     ArchiveService* archive_service,
                     Tabs& tabs,
                     Windows& windows,
                     TabStripApi& tab_strip_api,
                     Profiles& profiles,
                     Favicons& favicons,
                     AppLifecycle& app_lifecycle,
                     LibraryPorts& library_ports,
                     Shell& shell);
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
  void SeparateItem(const std::string& id) override;
  void ShowContextMenu(const std::vector<std::string>& labels,
                       int32_t x,
                       int32_t y,
                       ShowContextMenuCallback callback) override;
  void PlayHapticFeedback() override;
  void SetHomeToCurrent(const std::string& id) override;
  void SetCollapsed(const std::string& id, bool collapsed) override;
  void SetEssential(const std::string& id, bool essential) override;
  void NewTab() override;
  void ToggleCompactMode() override;
  void GetTabLifecycles(GetTabLifecyclesCallback callback) override;
  void KeepTab(const tabs_api::NodeId& tab) override;
  void ClearToday(ClearTodayCallback callback) override;
  void ReopenTabs(const std::vector<GURL>& urls) override;
  void GetSpaces(GetSpacesCallback callback) override;
  void GetSpaceSnapshot(const std::string& id,
                        GetSpaceSnapshotCallback callback) override;
  void SwitchToSpace(const std::string& id) override;
  void PrepareSpaceSwitch(const std::string& id) override;
  void CreateSpace(const std::string& name, const std::string& icon) override;
  void RenameSpace(const std::string& id, const std::string& name) override;
  void SetSpaceIcon(const std::string& id, const std::string& icon) override;
  void SetSpaceTheme(const std::string& id, const std::string& theme) override;
  void OpenThemePicker(const std::string& id) override;
  void SetLibraryWidth(int32_t width) override;
  void GetColumnInsets(GetColumnInsetsCallback callback) override;
  void BlendSpaceTheme(const std::string& theme, float progress) override;
  void ReorderSpaces(const std::vector<std::string>& ids) override;
  void SleepSpace(const std::string& id) override;
  void DeleteSpace(const std::string& id,
                   DeleteSpaceCallback callback) override;
  void SearchArchive(const std::string& query,
                     uint32_t limit,
                     SearchArchiveCallback callback) override;
  void RestoreFromArchive(int64_t id,
                          const GURL& url,
                          bool background) override;
  void RemoveFromArchive(int64_t id) override;
  void QueryHistory(const std::string& query,
                    int32_t days_old,
                    uint32_t limit,
                    mojom::HistorySort sort,
                    QueryHistoryCallback callback) override;
  void ForgetPage(const GURL& url) override;
  void ForgetSite(const GURL& url) override;
  void ClearHistory(double begin) override;
  void OpenUrl(const GURL& url, bool background) override;
  void GetLibrarySprite(const std::string& name,
                        GetLibrarySpriteCallback callback) override;
  void OpenSpaceEntry(const std::string& space_id,
                      const std::string& item_id,
                      const GURL& url) override;
  void GetDownloads(GetDownloadsCallback callback) override;
  void DoDownload(uint32_t id, mojom::DownloadCommand command) override;
  void ClearDownloads() override;
  void ScanMedia(const std::vector<std::string>& folders,
                 ScanMediaCallback callback) override;
  void DoMedia(uint32_t id, mojom::MediaCommand command) override;
  void GetMediaOpener(uint32_t id, GetMediaOpenerCallback callback) override;
  void GetRoutes(GetRoutesCallback callback) override;
  void AddRoute(const std::string& reference,
                const std::string& match,
                const std::string& space) override;
  void UpdateRoute(const std::string& id,
                   const std::string& reference,
                   const std::string& match,
                   const std::string& space) override;
  void RemoveRoute(const std::string& id) override;
  void SetDefaultRoute(const std::string& space) override;
  void GetICloudSync(const std::string& space,
                     GetICloudSyncCallback callback) override;
  void SetICloudSync(const std::string& space, bool enabled) override;
  void FindICloudSpaces(FindICloudSpacesCallback callback) override;
  void ConnectICloudSpace(mojom::ICloudSpacePtr space) override;
  void RestartBrowser() override;

  // SpaceController::Sidebar:
  void OnSpacesChanged() override;
  void OnSwitchRequested(const std::string& space_id) override;
  void OnSwipe(const SwipeTracker::Event& event) override;

 private:
  std::vector<mojom::SpacePtr> BuildSpaces();
  // Returns pinned items' live tabs inside `node` (itself included) to Today.
  void ReleaseLiveTabs(const Tree::Node& node);
  // Archives the pinned items of a folder that is being removed.
  void ArchiveFolder(const Tree::Node& folder,
                     const std::string& path);
  std::vector<mojom::TabLifecyclePtr> BuildLifecycles();
  void OnTreeChanged();
  void OnLifecyclesChanged();
  void OnContextMenuDone();

  class ContextMenu;

  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TreeService> tree_service_;
  raw_ptr<LifecycleService> lifecycle_service_;
  raw_ptr<ArchiveService> archive_service_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
  const raw_ref<TabStripApi> tab_strip_api_;
  const raw_ref<Profiles> profiles_;
  const raw_ref<Favicons> favicons_;
  const raw_ref<AppLifecycle> app_lifecycle_;
  const raw_ref<Shell> shell_;
  base::CallbackListSubscription tree_subscription_;
  base::CallbackListSubscription lifecycle_subscription_;
  std::unique_ptr<ContextMenu> context_menu_;
  std::unique_ptr<LibraryBackend> library_;
  std::unique_ptr<Downloads> downloads_;
  base::CancelableTaskTracker favicon_tracker_;
  base::WeakPtrFactory<SidebarPageHandler> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_PAGE_HANDLER_H_
