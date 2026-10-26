// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/tab_strip_api_adapter.h"

#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_interface.h"

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

}  // namespace cromarc
