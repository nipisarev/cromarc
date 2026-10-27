// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_bar.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/little/little_window.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"

namespace cromarc {

namespace {

constexpr int kIconSize = 16;
// Menu commands from here on are "always open in" the space at the offset.
constexpr int kAlwaysOpenCommand = 1000;

// A space's icon for a text menu: emoji or letter, nothing for Zen's icons.
std::u16string TextIcon(const std::string& icon) {
  return icon.empty() || icon.starts_with("icon:")
             ? std::u16string()
             : base::StrCat({base::UTF8ToUTF16(icon), u"  "});
}

std::u16string NameOf(const std::string& space_id) {
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (space.id == space_id) {
      return space.name;
    }
  }
  return std::u16string();
}

}  // namespace

LittleBar::LittleBar(LittleWindow* window) : window_(window->GetWeakPtr()) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 4), 2));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto open = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](base::WeakPtr<LittleWindow> window) {
            if (window) {
              window->OpenInSpace(window->default_space());
            }
          },
          window_),
      l10n_util::GetStringFUTF16(IDS_CROMARC_LITTLE_OPEN_IN,
                                 NameOf(window->default_space())));
  open->SetStyle(ui::ButtonStyle::kTonal);
  AddChildView(std::move(open));

  auto spaces = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&LittleBar::ShowSpacesMenu, base::Unretained(this)),
      vector_icons::kKeyboardArrowDownIcon, kIconSize);
  spaces->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_OPEN_IN_ANOTHER));
  menu_button_ = AddChildView(std::move(spaces));

  auto close = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&LittleWindow::Close, window_),
      vector_icons::kCloseIcon, kIconSize);
  close->SetTooltipText(l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_CLOSE));
  AddChildView(std::move(close));
}

LittleBar::~LittleBar() = default;

void LittleBar::ShowSpacesMenu() {
  menu_spaces_.clear();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  const std::vector<SpaceController::Space> spaces =
      SpaceController::Get()->GetSpaces();
  for (const SpaceController::Space& space : spaces) {
    menu_model_->AddItem(static_cast<int>(menu_spaces_.size()),
                         base::StrCat({TextIcon(space.icon), space.name}));
    menu_spaces_.push_back(space.id);
  }
  // Arc's Air Traffic Control, as a rule in the Library's links.
  const std::string host =
      window_ ? std::string(window_->url().host()) : std::string();
  if (!host.empty()) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    for (size_t i = 0; i < spaces.size(); ++i) {
      menu_model_->AddItem(
          kAlwaysOpenCommand + static_cast<int>(i),
          l10n_util::GetStringFUTF16(IDS_CROMARC_LITTLE_ALWAYS_OPEN,
                                     base::UTF8ToUTF16(host), spaces[i].name));
    }
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          menu_button_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kMouse);
}

void LittleBar::ExecuteCommand(int command_id, int event_flags) {
  if (!window_) {
    return;
  }
  const bool always = command_id >= kAlwaysOpenCommand;
  const size_t index =
      static_cast<size_t>(always ? command_id - kAlwaysOpenCommand : command_id);
  if (index >= menu_spaces_.size()) {
    return;
  }
  if (always) {
    SpaceRouting::Get()->AddRoute(std::string(window_->url().host()),
                                  Route::Match::kContains, menu_spaces_[index]);
  }
  window_->OpenInSpace(menu_spaces_[index]);
}

BEGIN_METADATA(LittleBar)
END_METADATA

}  // namespace cromarc
