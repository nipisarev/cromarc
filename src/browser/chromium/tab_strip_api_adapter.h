// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_TAB_STRIP_API_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_TAB_STRIP_API_ADAPTER_H_

#include "cromarc/browser/ports/tab_strip_api.h"

namespace cromarc {

// TabStripApi on tabs_api::NodeId and tab handles.
class TabStripApiAdapter : public TabStripApi {
 public:
  TabStripApiAdapter();
  TabStripApiAdapter(const TabStripApiAdapter&) = delete;
  TabStripApiAdapter& operator=(const TabStripApiAdapter&) = delete;
  ~TabStripApiAdapter() override;

  // TabStripApi:
  tabs_api::NodeId NodeIdOf(TabId tab) override;
  std::optional<TabId> TabOf(const tabs_api::NodeId& node) override;
  std::vector<tabs_api::mojom::TabPtr> Snapshot(
      BrowserWindowInterface* window) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_TAB_STRIP_API_ADAPTER_H_
