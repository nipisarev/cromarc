// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/tabs_adapter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_commands.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/browser_tabrestore.h"  // nogncheck: as above
#include "chrome/browser/ui/browser_tabstrip.h"  // nogncheck: as above
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/navigator/browser_navigator_params.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"  // nogncheck: as above
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

tabs::TabInterface* TabOf(TabId tab) {
  return tabs::TabHandle(tab.value()).Get();
}

TabId IdOf(const tabs::TabInterface* tab) {
  return TabId(tab->GetHandle().raw_value());
}

}  // namespace

// A window's tab strip events, from the first subscription until the window
// closes.
class TabsAdapter::WindowEvents : public TabStripModelObserver {
 public:
  WindowEvents(TabsAdapter& adapter, BrowserWindowInterface* window)
      : window_did_close_(window->RegisterBrowserDidClose(base::BindRepeating(
            &TabsAdapter::OnWindowClosed, base::Unretained(&adapter)))) {
    window->GetTabStripModel()->AddObserver(this);
  }
  ~WindowEvents() override = default;

  base::RepeatingClosureList& opened_in_background() {
    return opened_in_background_;
  }
  base::RepeatingClosureList& active_changed() { return active_changed_; }
  base::RepeatingCallbackList<void(TabId, std::optional<TabId>, bool)>&
  inserted() {
    return inserted_;
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const TabStripModelChange::ContentsWithIndex& inserted :
           change.GetInsert()->contents) {
        const tabs::TabInterface* opener =
            tab_strip_model->GetOpenerOfTabAt(inserted.index);
        inserted_.Notify(IdOf(inserted.tab),
                         opener ? std::make_optional(IdOf(opener))
                                : std::nullopt,
                         selection.active_tab_changed() &&
                             selection.new_tab == inserted.tab);
      }
      if (!selection.active_tab_changed()) {
        opened_in_background_.Notify();
      }
    }
    if (selection.active_tab_changed()) {
      active_changed_.Notify();
    }
  }

 private:
  base::RepeatingClosureList opened_in_background_;
  base::RepeatingClosureList active_changed_;
  base::RepeatingCallbackList<void(TabId, std::optional<TabId>, bool)>
      inserted_;
  base::CallbackListSubscription window_did_close_;
};

TabsAdapter::TabsAdapter() = default;
TabsAdapter::~TabsAdapter() = default;

bool TabsAdapter::Exists(TabId tab) {
  return TabOf(tab);
}

content::WebContents* TabsAdapter::GetContents(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  return interface ? interface->GetContents() : nullptr;
}

BrowserWindowInterface* TabsAdapter::GetWindow(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  return interface ? interface->GetBrowserWindowInterface() : nullptr;
}

std::optional<TabId> TabsAdapter::FromContents(content::WebContents* contents) {
  tabs::TabInterface* tab =
      contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
  if (!tab) {
    return std::nullopt;
  }
  return IdOf(tab);
}

GURL TabsAdapter::GetUrl(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  return contents ? contents->GetVisibleURL() : GURL();
}

GURL TabsAdapter::GetLastCommittedUrl(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  return contents ? contents->GetLastCommittedURL() : GURL();
}

std::u16string TabsAdapter::GetTitle(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  return contents ? contents->GetTitle() : std::u16string();
}

base::Time TabsAdapter::GetLastActiveTime(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  return contents ? contents->GetLastActiveTime() : base::Time();
}

bool TabsAdapter::IsAudibleOrCaptured(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  return contents &&
         (contents->IsCurrentlyAudible() || contents->IsBeingCaptured());
}

bool TabsAdapter::MayLoseInput(TabId tab) {
  content::WebContents* contents = GetContents(tab);
  if (!contents) {
    return false;
  }
  bool has_handler = false;
  contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&has_handler](content::RenderFrameHost* frame) {
        if (frame->GetSuddenTerminationDisablerState(
                blink::mojom::SuddenTerminationDisablerType::
                    kBeforeUnloadHandler)) {
          has_handler = true;
        }
      });
  if (!has_handler) {
    return false;
  }
  // Whether the user edited the page; when that cannot be known, they may
  // have.
  if (!performance_manager::PerformanceManager::IsAvailable()) {
    return true;
  }
  base::WeakPtr<performance_manager::PageNode> page =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          contents);
  return !page || page->HadUserEdits();
}

std::vector<TabId> TabsAdapter::List(BrowserWindowInterface* window) {
  TabStripModel* model = window->GetTabStripModel();
  std::vector<TabId> tabs;
  for (int i = 0; i < model->count(); ++i) {
    tabs.push_back(IdOf(model->GetTabAtIndex(i)));
  }
  return tabs;
}

std::optional<TabId> TabsAdapter::GetActive(BrowserWindowInterface* window) {
  tabs::TabInterface* tab = window->GetActiveTabInterface();
  if (!tab) {
    return std::nullopt;
  }
  return IdOf(tab);
}

bool TabsAdapter::IsPinned(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  return interface && interface->IsPinned();
}

void TabsAdapter::Activate(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* model =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  model->ActivateTabAt(model->GetIndexOfTab(interface));
}

void TabsAdapter::SetPinned(TabId tab, bool pinned) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* model =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(interface);
  if (index != TabStripModel::kNoTab && model->IsTabPinned(index) != pinned) {
    model->SetTabPinned(index, pinned);
  }
}

void TabsAdapter::Close(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* model =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(interface);
  if (index != TabStripModel::kNoTab) {
    model->CloseWebContentsAt(index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

void TabsAdapter::CloseByUser(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* model =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(interface);
  if (index != TabStripModel::kNoTab) {
    model->CloseWebContentsAt(index,
                              TabCloseTypes::CLOSE_USER_GESTURE |
                                  TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

std::optional<TabId> TabsAdapter::Duplicate(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return std::nullopt;
  }
  BrowserWindowInterface* window = interface->GetBrowserWindowInterface();
  return FromContents(chrome::DuplicateTabAt(
      window, window->GetTabStripModel()->GetIndexOfTab(interface)));
}

void TabsAdapter::Open(BrowserWindowInterface* window,
                       const GURL& url,
                       WindowOpenDisposition disposition) {
  window->OpenGURL(url, disposition);
}

void TabsAdapter::OpenNewTabPage(BrowserWindowInterface* window) {
  Open(window, GURL(chrome::kChromeUINewTabURL),
       WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void TabsAdapter::AddNewTabPage(BrowserWindowInterface* window,
                                bool keep_window_hidden) {
  chrome::AddTabAt(window, GURL(chrome::kChromeUINewTabURL), -1,
                   /*foreground=*/true, /*group=*/std::nullopt,
                   /*pinned=*/false,
                   keep_window_hidden
                       ? std::make_optional(
                             NavigateParams::WindowAction::kNoAction)
                       : std::nullopt);
}

std::optional<TabId> TabsAdapter::OpenPinned(BrowserWindowInterface* window,
                                             const GURL& url) {
  NavigateParams params(window, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_add_types = AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_PINNED;
  Navigate(&params);
  return FromContents(params.navigated_or_inserted_contents);
}

void TabsAdapter::AddUnloaded(BrowserWindowInterface* window,
                              const GURL& url,
                              const std::u16string& title) {
  sessions::SerializedNavigationEntry entry;
  entry.set_virtual_url(url);
  entry.set_title(title);
  entry.set_timestamp(base::Time::Now());
  chrome::AddRestoredTab(
      window, base::span_from_ref(entry),
      /*tab_index=*/window->GetTabStripModel()->count(),
      /*selected_navigation=*/0, /*extension_app_id=*/std::string(),
      /*group=*/std::nullopt, /*select=*/false, /*pin=*/false,
      base::TimeTicks::Now(), base::Time::Now(),
      /*storage_namespace=*/nullptr, sessions::SerializedUserAgentOverride(),
      /*extra_data=*/{}, /*from_session_restore=*/false,
      /*is_active_browser=*/std::nullopt);
}

std::optional<TabId> TabsAdapter::InsertAfterActive(
    BrowserWindowInterface* window,
    std::unique_ptr<content::WebContents> contents) {
  TabStripModel* model = window->GetTabStripModel();
  const int index = model->InsertWebContentsAt(
      model->active_index() + 1, std::move(contents),
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  tabs::TabInterface* tab = model->GetTabAtIndex(index);
  if (!tab) {
    return std::nullopt;
  }
  return IdOf(tab);
}

content::WebContents* TabsAdapter::AddNewContents(
    BrowserWindowInterface* window,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features) {
  return chrome::AddWebContents(window, source, std::move(new_contents),
                                target_url, disposition, window_features);
}

void TabsAdapter::MoveToWindow(TabId tab, BrowserWindowInterface* window) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* from =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  std::unique_ptr<tabs::TabModel> detached =
      from->DetachTabAtForInsertion(from->GetIndexOfTab(interface));
  TabStripModel* to = window->GetTabStripModel();
  to->InsertDetachedTabAt(to->count(), std::move(detached),
                          AddTabTypes::ADD_ACTIVE);
}

void TabsAdapter::OpenFrom(TabId tab, const content::OpenURLParams& params) {
  if (content::WebContents* contents = GetContents(tab)) {
    contents->OpenURL(params, /*navigation_handle_callback=*/{});
  }
}

void TabsAdapter::MoveToFirstUnpinned(TabId tab) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return;
  }
  TabStripModel* model =
      interface->GetBrowserWindowInterface()->GetTabStripModel();
  model->MoveWebContentsAt(model->GetIndexOfTab(interface),
                           model->IndexOfFirstNonPinnedTab(),
                           /*select_after_move=*/false);
}

TabsAdapter::WindowEvents& TabsAdapter::EventsOf(
    BrowserWindowInterface* window) {
  std::unique_ptr<WindowEvents>& events = events_[window];
  if (!events) {
    events = std::make_unique<WindowEvents>(*this, window);
  }
  return *events;
}

base::CallbackListSubscription TabsAdapter::OnOpenedInBackground(
    BrowserWindowInterface* window,
    base::RepeatingClosure callback) {
  return EventsOf(window).opened_in_background().Add(std::move(callback));
}

base::CallbackListSubscription TabsAdapter::OnInserted(
    BrowserWindowInterface* window,
    InsertedCallback callback) {
  return EventsOf(window).inserted().Add(std::move(callback));
}

base::CallbackListSubscription TabsAdapter::OnActiveChanged(
    BrowserWindowInterface* window,
    base::RepeatingClosure callback) {
  return EventsOf(window).active_changed().Add(std::move(callback));
}

base::CallbackListSubscription TabsAdapter::OnWillClose(
    TabId tab,
    base::RepeatingClosure callback) {
  tabs::TabInterface* interface = TabOf(tab);
  if (!interface) {
    return {};
  }
  return interface->RegisterWillDetach(base::BindRepeating(
      [](const base::RepeatingClosure& callback, tabs::TabInterface*,
         tabs::TabInterface::DetachReason reason) {
        if (reason == tabs::TabInterface::DetachReason::kDelete) {
          callback.Run();
        }
      },
      std::move(callback)));
}

void TabsAdapter::OnWindowClosed(BrowserWindowInterface* window) {
  events_.erase(window);
}

}  // namespace cromarc
