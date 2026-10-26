// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_TAB_STRIP_API_H_
#define CROMARC_BROWSER_PORTS_TAB_STRIP_API_H_

#include <optional>

#include "cromarc/browser/ports/tabs.h"

namespace tabs_api {
class NodeId;
}  // namespace tabs_api

namespace cromarc {

// Chrome's tab strip API (TabStripService), which the sidebar's page lists
// its window's tabs with: its ids of tabs. tabs_api::NodeId is mapped from
// Mojo, so the sidebar's interface carries it.
class TabStripApi {
 public:
  virtual ~TabStripApi() = default;

  virtual tabs_api::NodeId NodeIdOf(TabId tab) = 0;
  // Nullopt for a node that is not a tab.
  virtual std::optional<TabId> TabOf(const tabs_api::NodeId& node) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_TAB_STRIP_API_H_
