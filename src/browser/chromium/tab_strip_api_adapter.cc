// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/tab_strip_api_adapter.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/converters/tab_converters.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace cromarc {

TabStripApiAdapter::TabStripApiAdapter() = default;
TabStripApiAdapter::~TabStripApiAdapter() = default;

tabs_api::NodeId TabStripApiAdapter::NodeIdOf(TabId tab) {
  return tabs_api::NodeId::FromTabHandle(tabs::TabHandle(tab.value()));
}

std::optional<TabId> TabStripApiAdapter::TabOf(const tabs_api::NodeId& node) {
  std::optional<tabs::TabHandle> handle = node.ToTabHandle();
  if (!handle) {
    return std::nullopt;
  }
  return TabId(handle->raw_value());
}

std::vector<tabs_api::mojom::TabPtr> TabStripApiAdapter::Snapshot(
    BrowserWindowInterface* window) {
  std::vector<tabs_api::mojom::TabPtr> tabs;
  TabStripModel* model = window->GetTabStripModel();
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    tabs.push_back(tabs_api::converters::BuildMojoTab(
        tab, tab->GetContents()->GetColorProvider(),
        {.is_active = tab->IsActivated(), .is_selected = tab->IsSelected()}));
  }
  return tabs;
}

}  // namespace cromarc
