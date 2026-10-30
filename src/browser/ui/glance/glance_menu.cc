// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_menu.h"

#include <algorithm>

#include "cromarc/browser/ui/glance/glance_rules.h"

namespace cromarc {

bool OffersOpenLinkInGlance(const MenuContext& context) {
  return context.in_normal_tab && !context.in_glance &&
         OpensInGlance(context.link);
}

std::optional<size_t> OpenLinkInGlanceIndex(const std::vector<int>& commands,
                                            int first_open_command) {
  auto first = std::ranges::find(commands, first_open_command);
  if (first == commands.end()) {
    return std::nullopt;
  }
  auto separator = std::find(first, commands.end(), kMenuSeparator);
  return static_cast<size_t>(separator - commands.begin());
}

bool SearchOpensInGlance(const MenuContext& context) {
  return context.in_normal_tab && !context.in_glance;
}

}  // namespace cromarc
