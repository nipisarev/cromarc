// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace cromarc {

void InstallSidebar(views::View* parent,
                    views::View* native_tab_list,
                    BrowserWindowInterface* browser) {
  native_tab_list->SetVisible(false);
  for (views::View* child : parent->children()) {
    if (views::IsViewClass<SidebarView>(child)) {
      return;
    }
  }
  const std::optional<size_t> index = parent->GetIndexOf(native_tab_list);
  CHECK(index.has_value());
  parent->AddChildViewAt(std::make_unique<SidebarView>(browser), *index + 1);
}

}  // namespace cromarc
