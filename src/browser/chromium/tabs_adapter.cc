// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/tabs_adapter.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_commands.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/navigator/browser_navigator_params.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"  // nogncheck: as above
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
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

// Tells a window's subscribers of tabs inserted in the background, from the
// first subscription until the window closes.
class TabsAdapter::BackgroundOpenings : public TabStripModelObserver {
 public:
  BackgroundOpenings(TabsAdapter& adapter, BrowserWindowInterface* window)
      : window_did_close_(window->RegisterBrowserDidClose(base::BindRepeating(
            &TabsAdapter::OnWindowClosed, base::Unretained(&adapter)))) {
    window->GetTabStripModel()->AddObserver(this);
  }
  ~BackgroundOpenings() override = default;

  base::CallbackListSubscription Add(base::RepeatingClosure callback) {
    return callbacks_.Add(std::move(callback));
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted ||
        selection.active_tab_changed()) {
      return;
    }
    callbacks_.Notify();
  }

 private:
  base::RepeatingClosureList callbacks_;
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
                       bool background) {
  window->OpenGURL(url, background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                                   : WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void TabsAdapter::OpenNewTabPage(BrowserWindowInterface* window) {
  Open(window, GURL(chrome::kChromeUINewTabURL), /*background=*/false);
}

std::optional<TabId> TabsAdapter::OpenPinned(BrowserWindowInterface* window,
                                             const GURL& url) {
  NavigateParams params(window, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_add_types = AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_PINNED;
  Navigate(&params);
  return FromContents(params.navigated_or_inserted_contents);
}

base::CallbackListSubscription TabsAdapter::OnOpenedInBackground(
    BrowserWindowInterface* window,
    base::RepeatingClosure callback) {
  std::unique_ptr<BackgroundOpenings>& openings =
      background_openings_[window];
  if (!openings) {
    openings = std::make_unique<BackgroundOpenings>(*this, window);
  }
  return openings->Add(std::move(callback));
}

void TabsAdapter::OnWindowClosed(BrowserWindowInterface* window) {
  background_openings_.erase(window);
}

}  // namespace cromarc
