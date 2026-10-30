// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_LITTLE_LITTLE_BAR_H_
#define CROMARC_BROWSER_UI_LITTLE_LITTLE_BAR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
class MenuRunner;
}  // namespace views

namespace cromarc {

class LittleWindow;

// The right end of a Little window's toolbar: its space (a menu moves the
// window to another), "Open in <space>", a menu of the other spaces, and
// close.
class LittleBar : public views::View, public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(LittleBar, views::View)

 public:
  explicit LittleBar(LittleWindow* window);
  LittleBar(const LittleBar&) = delete;
  LittleBar& operator=(const LittleBar&) = delete;
  ~LittleBar() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void ShowSpacesMenu();
  void ShowMoveMenu();

  // The window outlives its bar only until it closes.
  base::WeakPtr<LittleWindow> window_;
  raw_ptr<views::View> menu_button_ = nullptr;
  raw_ptr<views::View> space_button_ = nullptr;
  std::vector<std::string> menu_spaces_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_LITTLE_LITTLE_BAR_H_
