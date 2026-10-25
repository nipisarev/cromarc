// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_page_handler.h"

#include <algorithm>
#include <set>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/prefs/pref_service.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/lifecycle/lifecycle_service.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/sync/icloud_sync.h"
#include "cromarc/browser/tree/tree.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/browser_commands.h"
#include "cromarc/browser/ui/sidebar/compact_mode.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "cromarc/browser/ui/sidebar/haptic_feedback.h"
#endif

namespace cromarc {

namespace {

tabs::TabInterface* TabFromNodeId(const tabs_api::NodeId& id) {
  std::optional<tabs::TabHandle> handle = id.ToTabHandle();
  return handle ? handle->Get() : nullptr;
}

void SetPinned(tabs::TabInterface* tab, bool pinned) {
  TabStripModel* model = tab->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(tab);
  if (index != TabStripModel::kNoTab && model->IsTabPinned(index) != pinned) {
    model->SetTabPinned(index, pinned);
  }
}

std::vector<mojom::TreeNodePtr> BuildChildren(
    TreeService* tree_service,
    const bookmarks::BookmarkNode* folder) {
  std::vector<mojom::TreeNodePtr> nodes;
  for (const auto& child : folder->children()) {
    auto node = mojom::TreeNode::New();
    node->id = Tree::IdOf(child.get());
    node->title = base::UTF16ToUTF8(child->GetTitle());
    node->is_folder = child->is_folder();
    if (child->is_folder()) {
      node->collapsed = Tree::IsCollapsed(child.get());
      node->children = BuildChildren(tree_service, child.get());
    } else {
      node->essential = Tree::IsEssential(child.get());
      node->home_url = child->url();
      if (tabs::TabInterface* tab = tree_service->GetLiveTab(node->id)) {
        node->tab = tabs_api::NodeId::FromTabHandle(tab->GetHandle());
      }
    }
    nodes.push_back(std::move(node));
  }
  return nodes;
}

std::string FaviconDataUrl(content::WebContents* contents) {
  auto* driver = favicon::ContentFaviconDriver::FromWebContents(contents);
  if (!driver) {
    return std::string();
  }
  const gfx::Image image = driver->GetFavicon();
  return image.IsEmpty() ? std::string()
                         : webui::GetBitmapDataUrl(*image.ToSkBitmap());
}

}  // namespace

// Zen's and Arc's context menus are native popups over the window, not boxes
// inside the sidebar; so are these. On the Mac the menu is an NSMenu that runs
// inside Run(): the chosen command arrives before the menu closes.
class SidebarPageHandler::ContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  ContextMenu(const std::vector<std::string>& labels,
              ShowContextMenuCallback callback,
              base::OnceClosure done)
      : callback_(std::move(callback)), done_(std::move(done)) {
    const auto is_child = [&labels](size_t i) {
      return i < labels.size() && labels[i].starts_with('\t');
    };
    for (size_t i = 0; i < labels.size(); ++i) {
      if (is_child(i)) {
        continue;
      }
      if (!is_child(i + 1)) {
        Add(model_, i, labels[i]);
        continue;
      }
      // The labels after it that start with a tab are its submenu.
      auto submenu = std::make_unique<ui::SimpleMenuModel>(this);
      for (size_t j = i + 1; is_child(j); ++j) {
        Add(*submenu, j, std::string_view(labels[j]).substr(1));
      }
      model_.AddSubMenu(static_cast<int>(i), base::UTF8ToUTF16(labels[i]),
                        submenu.get());
      submenus_.push_back(std::move(submenu));
    }
  }

  void Run(views::View* anchor, gfx::Point point) {
    views::View::ConvertPointToScreen(anchor, &point);
    runner_ = std::make_unique<views::MenuRunner>(
        &model_, views::MenuRunner::CONTEXT_MENU,
        base::BindRepeating(&ContextMenu::OnClosed, base::Unretained(this)));
    runner_->RunMenuAt(anchor->GetWidget(), nullptr,
                       gfx::Rect(point, gfx::Size()),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kMouse);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    chosen_ = command_id;
  }
  bool IsCommandIdChecked(int command_id) const override {
    return checked_.contains(command_id);
  }

 private:
  void Add(ui::SimpleMenuModel& model, size_t index, std::string_view label) {
    if (label.empty()) {
      model.AddSeparator(ui::NORMAL_SEPARATOR);
      return;
    }
    const int id = static_cast<int>(index);
    if (label.starts_with('\x01')) {
      model.AddCheckItem(id, base::UTF8ToUTF16(label.substr(1)));
      checked_.insert(id);
      return;
    }
    model.AddItem(id, base::UTF8ToUTF16(label));
  }

  void OnClosed() {
    if (callback_) {
      std::move(callback_).Run(chosen_);
    }
    // The runner is still on the stack.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(done_));
  }

  ui::SimpleMenuModel model_{this};
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> submenus_;
  std::set<int> checked_;
  std::unique_ptr<views::MenuRunner> runner_;
  ShowContextMenuCallback callback_;
  base::OnceClosure done_;
  int chosen_ = -1;
};

SidebarPageHandler::SidebarPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page,
    BrowserWindowInterface* browser,
    TreeService* tree_service,
    LifecycleService* lifecycle_service,
    ArchiveService* archive_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser),
      tree_service_(tree_service),
      lifecycle_service_(lifecycle_service),
      archive_service_(archive_service),
      tree_subscription_(tree_service_->RegisterOnChanged(
          base::BindRepeating(&SidebarPageHandler::OnTreeChanged,
                              base::Unretained(this)))),
      lifecycle_subscription_(lifecycle_service_->RegisterOnChanged(
          base::BindRepeating(&SidebarPageHandler::OnLifecyclesChanged,
                              base::Unretained(this)))),
      library_(std::make_unique<LibraryBackend>(
          browser_,
          archive_service_,
          base::BindRepeating(
              [](SidebarPageHandler* self) { self->page_->OnHistoryChanged(); },
              base::Unretained(this)))) {
  SpaceController::Get()->AddSidebar(browser_, this);
}

SidebarPageHandler::~SidebarPageHandler() {
  SpaceController::Get()->RemoveSidebar(browser_);
}

void SidebarPageHandler::GetTree(GetTreeCallback callback) {
  if (!tree_service_->loaded()) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(BuildChildren(tree_service_, tree_service_->tree().Root()));
}

void SidebarPageHandler::OnTreeChanged() {
  if (tree_service_->loaded()) {
    page_->OnTreeChanged(BuildChildren(tree_service_, tree_service_->tree().Root()));
  }
}

void SidebarPageHandler::OpenItem(const std::string& id) {
  tree_service_->Open(browser_, id);
}

void SidebarPageHandler::CreateFolder(const std::string& parent_id,
                                      uint32_t index,
                                      const std::string& title,
                                      CreateFolderCallback callback) {
  const bookmarks::BookmarkNode* folder = tree_service_->tree().CreateFolder(
      parent_id, index, base::UTF8ToUTF16(title));
  std::move(callback).Run(folder ? Tree::IdOf(folder) : std::string());
}

void SidebarPageHandler::PinTab(const tabs_api::NodeId& tab_id,
                                const std::string& parent_id,
                                uint32_t index,
                                bool essential) {
  tabs::TabInterface* tab = TabFromNodeId(tab_id);
  if (!tab || tree_service_->GetItemForTab(tab)) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  Tree& tree = tree_service_->tree();
  const bookmarks::BookmarkNode* node = tree.AddPinned(
      parent_id, index, contents->GetTitle(), contents->GetLastCommittedURL());
  if (!node) {
    return;
  }
  const std::string id = Tree::IdOf(node);
  if (essential) {
    tree.SetEssential(id, true);
  }
  SetPinned(tab, true);
  tree_service_->Link(id, tab);
}

void SidebarPageHandler::MoveItem(const std::string& id,
                                  const std::string& parent_id,
                                  uint32_t index) {
  tree_service_->tree().Move(id, parent_id, index);
}

void SidebarPageHandler::RenameItem(const std::string& id,
                                    const std::string& title) {
  tree_service_->tree().Rename(id, base::UTF8ToUTF16(title));
}

void SidebarPageHandler::ReleaseLiveTabs(const bookmarks::BookmarkNode* node) {
  if (node->is_url()) {
    const std::string id = Tree::IdOf(node);
    if (tabs::TabInterface* tab = tree_service_->GetLiveTab(id)) {
      SetPinned(tab, false);
      tree_service_->Unlink(id);
    }
    return;
  }
  for (const auto& child : node->children()) {
    ReleaseLiveTabs(child.get());
  }
}

void SidebarPageHandler::ArchiveFolder(const bookmarks::BookmarkNode* folder,
                                       const std::string& path) {
  const std::string here =
      path.empty() ? base::UTF16ToUTF8(folder->GetTitle())
                   : path + " / " + base::UTF16ToUTF8(folder->GetTitle());
  for (const auto& child : folder->children()) {
    if (child->is_folder()) {
      ArchiveFolder(child.get(), here);
      continue;
    }
    ArchiveEntry entry;
    entry.url = child->url();
    entry.title = child->GetTitle();
    entry.closed_at = base::Time::Now();
    entry.reason = "folder";
    entry.folder_path = here;
    archive_service_->Add(std::move(entry));
  }
}

void SidebarPageHandler::RemoveItem(const std::string& id) {
  const bookmarks::BookmarkNode* node = tree_service_->tree().Find(id);
  if (!node) {
    return;
  }
  ReleaseLiveTabs(node);
  // A removed folder's pinned items stay findable in the archive (design,
  // UI section 3).
  if (node->is_folder()) {
    ArchiveFolder(node, std::string());
  }
  tree_service_->tree().Remove(id);
}

void SidebarPageHandler::ResetItem(const std::string& id) {
  const bookmarks::BookmarkNode* node = tree_service_->tree().Find(id);
  tabs::TabInterface* tab = tree_service_->GetLiveTab(id);
  if (!node || !node->is_url() || !tab) {
    return;
  }
  // As Zen's reset (ZenPinnedTabManager #resetTabToStoredState): only the
  // pinned entry stays in the tab's history.
  content::NavigationController::LoadURLParams params(node->url());
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.should_clear_history_list = true;
  tab->GetContents()->GetController().LoadURLWithParams(params);
}

void SidebarPageHandler::SeparateItem(const std::string& id) {
  tabs::TabInterface* tab = tree_service_->GetLiveTab(id);
  if (!tab) {
    return;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  TabStripModel* model = browser->GetTabStripModel();
  // The copy keeps the page and its history as an ordinary tab, loaded at
  // once as Zen's duplicateTab(tab, true).
  if (content::WebContents* copy =
          chrome::DuplicateTabAt(browser, model->GetIndexOfTab(tab))) {
    if (tabs::TabInterface* copy_tab =
            tabs::TabInterface::GetFromContents(copy)) {
      SetPinned(copy_tab, false);
    }
    copy->GetController().LoadIfNecessary();
  }
  ResetItem(id);
  model->ActivateTabAt(model->GetIndexOfTab(tab));
}

void SidebarPageHandler::SetHomeToCurrent(const std::string& id) {
  if (tabs::TabInterface* tab = tree_service_->GetLiveTab(id)) {
    tree_service_->tree().SetHomeUrl(
        id, tab->GetContents()->GetLastCommittedURL());
  }
}

void SidebarPageHandler::ShowContextMenu(const std::vector<std::string>& labels,
                                         int32_t x,
                                         int32_t y,
                                         ShowContextMenuCallback callback) {
  SidebarView* view = SidebarView::ForBrowser(browser_);
  if (!view || !view->GetWidget() || context_menu_) {
    std::move(callback).Run(-1);
    return;
  }
  context_menu_ = std::make_unique<ContextMenu>(
      labels, std::move(callback),
      base::BindOnce(&SidebarPageHandler::OnContextMenuDone,
                     weak_factory_.GetWeakPtr()));
  context_menu_->Run(view, gfx::Point(x, y));
}

void SidebarPageHandler::OnContextMenuDone() {
  context_menu_.reset();
}

void SidebarPageHandler::PlayHapticFeedback() {
#if BUILDFLAG(IS_MAC)
  PerformHapticFeedback();
#endif
}

void SidebarPageHandler::SetCollapsed(const std::string& id, bool collapsed) {
  tree_service_->tree().SetCollapsed(id, collapsed);
}

void SidebarPageHandler::SetEssential(const std::string& id, bool essential) {
  tree_service_->tree().SetEssential(id, essential);
}

void SidebarPageHandler::ToggleCompactMode() {
  CompactMode::Toggle(browser_);
}

void SidebarPageHandler::NewTab() {
  // Same as Cmd+T (design, UI section 4).
  if (!ShowCommandBar(browser_, /*edit_current_url=*/false)) {
    browser_->OpenGURL(GURL(chrome::kChromeUINewTabURL),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

std::vector<mojom::TabLifecyclePtr> SidebarPageHandler::BuildLifecycles() {
  std::vector<mojom::TabLifecyclePtr> result;
  for (const LifecycleService::TabState& state :
       lifecycle_service_->GetTabStates(browser_)) {
    auto lifecycle = mojom::TabLifecycle::New();
    lifecycle->tab = tabs_api::NodeId::FromTabHandle(state.tab->GetHandle());
    lifecycle->deferred = state.deferred;
    lifecycle->minutes_left =
        static_cast<int32_t>(state.time_left.InMinutes());
    result.push_back(std::move(lifecycle));
  }
  return result;
}

void SidebarPageHandler::GetTabLifecycles(GetTabLifecyclesCallback callback) {
  std::move(callback).Run(BuildLifecycles());
}

void SidebarPageHandler::OnLifecyclesChanged() {
  page_->OnTabLifecyclesChanged(BuildLifecycles());
}

void SidebarPageHandler::KeepTab(const tabs_api::NodeId& tab_id) {
  if (tabs::TabInterface* tab = TabFromNodeId(tab_id)) {
    lifecycle_service_->Keep(tab);
  }
}

void SidebarPageHandler::ClearToday(ClearTodayCallback callback) {
  std::move(callback).Run(lifecycle_service_->ClearToday(browser_));
}

void SidebarPageHandler::ReopenTabs(const std::vector<GURL>& urls) {
  for (const GURL& url : urls) {
    browser_->OpenGURL(url, WindowOpenDisposition::NEW_BACKGROUND_TAB);
    // Open again, so not archived: as restoring from the Library.
    archive_service_->RemoveUrl(url);
  }
}

std::vector<mojom::SpacePtr> SidebarPageHandler::BuildSpaces() {
  const std::string current = SpaceController::IdOf(browser_->GetProfile());
  std::vector<mojom::SpacePtr> result;
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    auto s = mojom::Space::New();
    s->id = space.id;
    s->name = base::UTF16ToUTF8(space.name);
    s->icon = space.icon;
    s->color = space.color;
    s->current = space.id == current;
    s->theme = space.theme;
    result.push_back(std::move(s));
  }
  return result;
}

void SidebarPageHandler::GetSpaces(GetSpacesCallback callback) {
  std::move(callback).Run(BuildSpaces());
}

void SidebarPageHandler::GetSpaceSnapshot(const std::string& id,
                                          GetSpaceSnapshotCallback callback) {
  Profile* profile = SpaceController::Get()->GetLoadedProfile(id);
  TreeService* tree_service = profile ? TreeService::Get(profile) : nullptr;
  if (!tree_service || !tree_service->loaded()) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto snapshot = mojom::SpaceSnapshot::New();
  snapshot->tree = BuildChildren(tree_service, tree_service->tree().Root());
  if (BrowserWindowInterface* window = SpaceController::GetWindowFor(profile)) {
    TabStripModel* model = window->GetTabStripModel();
    // Today, newest first, as the sidebar shows it.
    for (int i = model->count() - 1; i >= 0; --i) {
      tabs::TabInterface* tab = model->GetTabAtIndex(i);
      if (tree_service->GetItemForTab(tab)) {
        continue;
      }
      content::WebContents* contents = tab->GetContents();
      auto snapshot_tab = mojom::SnapshotTab::New();
      snapshot_tab->title = base::UTF16ToUTF8(contents->GetTitle());
      snapshot_tab->url = contents->GetLastCommittedURL();
      snapshot_tab->favicon = FaviconDataUrl(contents);
      snapshot->today.push_back(std::move(snapshot_tab));
    }
  }
  std::move(callback).Run(std::move(snapshot));
}

void SidebarPageHandler::SwitchToSpace(const std::string& id) {
  SpaceController::Get()->SwitchTo(browser_, id);
}

void SidebarPageHandler::PrepareSpaceSwitch(const std::string& id) {
  SpaceController::Get()->PrepareSwitch(browser_, id);
}

void SidebarPageHandler::CreateSpace(const std::string& name,
                                     const std::string& icon) {
  SpaceController::Get()->CreateSpace(browser_, base::UTF8ToUTF16(name), icon);
}

void SidebarPageHandler::RenameSpace(const std::string& id,
                                     const std::string& name) {
  SpaceController::Get()->RenameSpace(id, base::UTF8ToUTF16(name));
}

void SidebarPageHandler::SetSpaceIcon(const std::string& id,
                                      const std::string& icon) {
  SpaceController::Get()->SetSpaceIcon(id, icon);
}

void SidebarPageHandler::SetSpaceTheme(const std::string& id,
                                       const std::string& theme) {
  SpaceController::Get()->SetSpaceTheme(id, theme);
}

void SidebarPageHandler::PreviewSpaceTheme(const std::string& theme) {
  if (SidebarView* sidebar = SidebarView::ForBrowser(browser_)) {
    sidebar->SetThemePreview(theme.empty() ? std::nullopt
                                           : std::make_optional(theme));
  }
}

void SidebarPageHandler::SetLibraryWidth(int32_t width) {
  if (SidebarView* sidebar = SidebarView::ForBrowser(browser_)) {
    sidebar->SetLibraryWidth(std::max(0, width));
  }
}

void SidebarPageHandler::BlendSpaceTheme(const std::string& theme,
                                         float progress) {
  if (SidebarView* sidebar = SidebarView::ForBrowser(browser_)) {
    sidebar->SetThemeBlend(progress > 0 ? std::make_optional<SidebarView::Blend>(
                                              theme, progress)
                                        : std::nullopt);
  }
}

void SidebarPageHandler::ReorderSpaces(const std::vector<std::string>& ids) {
  SpaceController::Get()->ReorderSpaces(ids);
}

void SidebarPageHandler::SleepSpace(const std::string& id) {
  // A space whose profile is not loaded has nothing live.
  Profile* profile = SpaceController::Get()->GetLoadedProfile(id);
  LifecycleService* lifecycle = profile ? LifecycleService::Get(profile) : nullptr;
  TreeService* tree = profile ? TreeService::Get(profile) : nullptr;
  if (!lifecycle || !tree) {
    return;
  }
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetProfile() != profile ||
        window->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
      continue;
    }
    // The window keeps a new tab; the rest goes.
    window->OpenGURL(GURL(chrome::kChromeUINewTabURL),
                     WindowOpenDisposition::NEW_FOREGROUND_TAB);
    TabStripModel* model = window->GetTabStripModel();
    for (int i = model->count() - 1; i >= 0; --i) {
      if (tree->GetItemForTab(model->GetTabAtIndex(i))) {
        model->CloseWebContentsAt(i,
                                  TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      }
    }
    lifecycle->ClearToday(window);
  }
}

void SidebarPageHandler::DeleteSpace(const std::string& id,
                                     DeleteSpaceCallback callback) {
  std::move(callback).Run(SpaceController::Get()->DeleteSpace(browser_, id));
}

void SidebarPageHandler::SearchArchive(const std::string& query,
                                       uint32_t limit,
                                       SearchArchiveCallback callback) {
  archive_service_->Search(
      base::UTF8ToUTF16(query), limit,
      base::BindOnce(
          [](SearchArchiveCallback callback,
             std::vector<ArchiveEntry> entries) {
            std::vector<mojom::ArchivedTabPtr> tabs;
            for (const ArchiveEntry& entry : entries) {
              tabs.push_back(mojom::ArchivedTab::New(
                  entry.id, entry.url, base::UTF16ToUTF8(entry.title),
                  entry.closed_at.InMillisecondsFSinceUnixEpoch(),
                  entry.reason, entry.folder_path));
            }
            std::move(callback).Run(std::move(tabs));
          },
          std::move(callback)));
}

void SidebarPageHandler::RestoreFromArchive(int64_t id,
                                            const GURL& url,
                                            bool background) {
  library_->Restore(id, url, background);
}

void SidebarPageHandler::RemoveFromArchive(int64_t id) {
  archive_service_->Remove(id);
}

void SidebarPageHandler::QueryHistory(const std::string& query,
                                      int32_t days_old,
                                      uint32_t limit,
                                      mojom::HistorySort sort,
                                      QueryHistoryCallback callback) {
  library_->QueryHistory(query, days_old, limit, sort, std::move(callback));
}

void SidebarPageHandler::ForgetPage(const GURL& url) {
  library_->ForgetPage(url);
}

void SidebarPageHandler::ForgetSite(const GURL& url) {
  library_->ForgetSite(url);
}

void SidebarPageHandler::ClearHistory(double begin) {
  library_->ClearHistory(
      begin > 0 ? base::Time::FromMillisecondsSinceUnixEpoch(begin)
                : base::Time());
}

void SidebarPageHandler::OpenUrl(const GURL& url, bool background) {
  library_->OpenUrl(url, background);
}

void SidebarPageHandler::GetLibrarySprite(const std::string& name,
                                          GetLibrarySpriteCallback callback) {
  const std::string path = "sidebar/library_icons/" + name + ".svg";
  for (const webui::ResourcePath& resource : kCromarcResources) {
    if (resource.path == path) {
      std::move(callback).Run(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
              resource.id));
      return;
    }
  }
  std::move(callback).Run(std::string());
}

void SidebarPageHandler::GetRoutes(GetRoutesCallback callback) {
  SpaceRouting::Get()->WhenLoaded(base::BindOnce(
      [](GetRoutesCallback callback) {
        const Routes& routes = SpaceRouting::Get()->routes();
        std::vector<mojom::LinkRoutePtr> result;
        for (const Route& route : routes.routes) {
          result.push_back(mojom::LinkRoute::New(
              route.id, route.reference, std::string(MatchName(route.match)),
              route.space));
        }
        std::move(callback).Run(std::move(result), routes.default_external);
      },
      std::move(callback)));
}

void SidebarPageHandler::AddRoute(const std::string& reference,
                                  const std::string& match,
                                  const std::string& space) {
  const Route::Match type =
      MatchFromName(match).value_or(Route::Match::kContains);
  SpaceRouting::Get()->WhenLoaded(base::BindOnce(
      [](std::string reference, Route::Match type, std::string space) {
        SpaceRouting::Get()->AddRoute(reference, type, space);
      },
      reference, type, space));
}

void SidebarPageHandler::UpdateRoute(const std::string& id,
                                     const std::string& reference,
                                     const std::string& match,
                                     const std::string& space) {
  const Route::Match type =
      MatchFromName(match).value_or(Route::Match::kContains);
  SpaceRouting::Get()->WhenLoaded(base::BindOnce(
      [](std::string id, std::string reference, Route::Match type,
         std::string space) {
        SpaceRouting::Get()->UpdateRoute(id, reference, type, space);
      },
      id, reference, type, space));
}

void SidebarPageHandler::RemoveRoute(const std::string& id) {
  SpaceRouting::Get()->RemoveRoute(id);
}

void SidebarPageHandler::SetDefaultRoute(const std::string& space) {
  SpaceRouting::Get()->SetDefaultExternal(space);
}

void SidebarPageHandler::GetICloudSync(const std::string& space,
                                       GetICloudSyncCallback callback) {
  SpaceController::Get()->LoadSpace(
      space, base::BindOnce(
                 [](GetICloudSyncCallback callback, Profile* profile) {
                   ICloudSync* sync = profile ? ICloudSync::Get(profile) : nullptr;
                   std::move(callback).Run(sync && sync->enabled(),
                                           sync && sync->active());
                 },
                 std::move(callback)));
}

void SidebarPageHandler::SetICloudSync(const std::string& space,
                                       bool enabled) {
  SpaceController::Get()->LoadSpace(
      space, base::BindOnce(
                 [](bool enabled, Profile* profile) {
                   if (ICloudSync* sync =
                           profile ? ICloudSync::Get(profile) : nullptr) {
                     sync->SetEnabled(enabled);
                   }
                 },
                 enabled));
}

void SidebarPageHandler::FindICloudSpaces(FindICloudSpacesCallback callback) {
  // Spaces this Mac already has, by the uuid they sync under.
  std::set<std::string> local;
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (Profile* profile = SpaceController::Get()->GetLoadedProfile(space.id)) {
      local.insert(profile->GetPrefs()->GetString(kSpaceUuidPref));
    }
  }
  ICloudSync::FindSpaces(base::BindOnce(
      [](std::set<std::string> local, FindICloudSpacesCallback callback,
         std::vector<ICloudSync::RemoteSpace> found) {
        std::vector<mojom::ICloudSpacePtr> spaces;
        for (const ICloudSync::RemoteSpace& space : found) {
          if (!local.contains(space.uuid)) {
            spaces.push_back(
                mojom::ICloudSpace::New(space.uuid, space.name, space.icon));
          }
        }
        std::move(callback).Run(std::move(spaces));
      },
      std::move(local), std::move(callback)));
}

void SidebarPageHandler::ConnectICloudSpace(mojom::ICloudSpacePtr space) {
  SpaceController::Get()->CreateSpace(
      browser_, base::UTF8ToUTF16(space->name), space->icon,
      base::BindOnce(
          [](std::string uuid, Profile* profile) {
            if (ICloudSync* sync = ICloudSync::Get(profile)) {
              sync->SetUuid(uuid);
              sync->SetEnabled(true);
            }
          },
          space->uuid));
}

void SidebarPageHandler::RestartBrowser() {
  chrome::AttemptRestart();
}

void SidebarPageHandler::OnSpacesChanged() {
  page_->OnSpacesChanged(BuildSpaces());
}

void SidebarPageHandler::OnSwitchRequested(const std::string& space_id) {
  page_->OnSwitchRequested(space_id);
}

void SidebarPageHandler::OnSwipe(const SwipeTracker::Event& event) {
  auto swipe = mojom::Swipe::New();
  switch (event.type) {
    case SwipeTracker::EventType::kStart:
      swipe->type = mojom::SwipeType::kStart;
      break;
    case SwipeTracker::EventType::kUpdate:
      swipe->type = mojom::SwipeType::kUpdate;
      break;
    case SwipeTracker::EventType::kEnd:
      swipe->type = mojom::SwipeType::kEnd;
      break;
  }
  swipe->amount = event.amount;
  swipe->direction = event.direction;
  page_->OnSwipe(std::move(swipe));
}

}  // namespace cromarc
