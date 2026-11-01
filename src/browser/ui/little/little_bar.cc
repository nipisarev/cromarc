// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_bar.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/routing/space_routing.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/little/little_window.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/hit_test_utils.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// Sizes measured on Arc's Little Arc, at 1x. The bar itself is
// kLittleBarHeight.
//
// Room for the traffic lights: Chromium's own caption insets on macOS 26
// (browser_frame_view_mac.mm, MacTabStripInsets: 12 + 62 + 12), which is
// where the window puts them (browser_native_widget_window_mac.mm).
constexpr int kTrafficLightsRoom = 86;
constexpr int kEndMargin = 8;
constexpr int kGap = 8;
// The field holding the space, the domain and Copy Link; the Open button has
// its height.
constexpr int kFieldHeight = 30;
constexpr int kFieldRadius = 8;
constexpr int kFieldInset = 4;
constexpr int kChipHeight = 22;
constexpr int kChipPadding = 8;
constexpr int kIconButtonSize = 26;
constexpr int kIconSize = 16;
constexpr int kButtonPadding = 12;
constexpr int kKeyRadius = 4;
constexpr base::TimeDelta kCopiedFor = base::Milliseconds(1500);

// Menu commands from here on are "always open in" the space at the offset.
constexpr int kAlwaysOpenCommand = 1000;
// And from here on, "move this window to" the space at the offset.
constexpr int kMoveWindowCommand = 2000;

// Tests can't see native views: with this switch the bar says what it shows.
constexpr char kLogLittleSwitch[] = "cromarc-test-log-little";

std::optional<SpaceController::Space> SpaceOf(const std::string& space_id) {
  for (SpaceController::Space& space : SpaceController::Get()->GetSpaces()) {
    if (space.id == space_id) {
      return std::move(space);
    }
  }
  return std::nullopt;
}

// A space's icon for text: emoji or letter, nothing for Zen's icons.
std::u16string TextIcon(const std::string& icon) {
  return icon.empty() || icon.starts_with("icon:")
             ? std::u16string()
             : base::StrCat({base::UTF8ToUTF16(icon), u" "});
}

std::u16string LabelOf(const SpaceController::Space& space) {
  return base::StrCat({TextIcon(space.icon), space.name});
}

// Arc shows the host without "www.".
std::u16string DomainOf(const GURL& url) {
  if (!url.has_host()) {
    return base::UTF8ToUTF16(url.spec());
  }
  std::string_view host = url.host();
  if (host.starts_with("www.")) {
    host.remove_prefix(4);
  }
  return base::UTF8ToUTF16(host);
}

gfx::FontList Font(gfx::Font::Weight weight, int size_delta = 0) {
  return gfx::FontList().DeriveWithSizeDelta(size_delta).DeriveWithWeight(
      weight);
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  return label;
}

void Clickable(views::View* view) {
  views::SetHitTestComponent(view, HTCLIENT);
}

// Arc's space chip and "Open in <space>" button: a rounded fill, lighter
// under the pointer, around a row of labels.
class PillButton : public views::Button {
  METADATA_HEADER(PillButton, views::Button)

 public:
  PillButton(PressedCallback callback, int radius, int padding)
      : views::Button(std::move(callback)), radius_(radius) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, padding)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
  }

  void SetFills(SkColor normal, SkColor hovered) {
    normal_ = normal;
    hovered_ = hovered;
    UpdateFill();
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override { UpdateFill(); }

 private:
  void UpdateFill() {
    SetBackground(views::CreateRoundedRectBackground(
        GetState() == STATE_NORMAL ? normal_ : hovered_, radius_));
  }

  const int radius_;
  SkColor normal_ = SK_ColorTRANSPARENT;
  SkColor hovered_ = SK_ColorTRANSPARENT;
};

BEGIN_METADATA(PillButton)
END_METADATA

}  // namespace

LittleBar::LittleBar(LittleWindow* window) : window_(window->GetWeakPtr()) {
  // The bar's empty space and the field's moves the window (H25).
  views::SetHitTestComponent(this, HTCAPTION);
  const std::optional<SpaceController::Space> space =
      SpaceOf(window->default_space());
  if (space) {
    theme_ = ParseSpaceTheme(space->theme);
  }

  auto field = std::make_unique<views::View>();
  field_ = AddChildView(std::move(field));

  // Arc's chip: the window's space; its menu moves the window to another.
  auto chip = std::make_unique<PillButton>(
      base::BindRepeating(&LittleBar::ShowMoveMenu, base::Unretained(this)),
      kChipHeight / 2, kChipPadding);
  space_label_ = chip->AddChildView(
      MakeLabel(space ? LabelOf(*space) : std::u16string(),
                Font(gfx::Font::Weight::MEDIUM)));
  space_label_->SetCanProcessEventsWithinSubtree(false);
  chip->GetViewAccessibility().SetName(std::u16string(space_label_->GetText()));
  chip->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_MOVE_WINDOW_TOOLTIP));
  Clickable(chip.get());
  space_ = field_->AddChildView(std::move(chip));

  domain_ = field_->AddChildView(
      MakeLabel(std::u16string(), Font(gfx::Font::Weight::NORMAL)));
  domain_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  domain_->SetElideBehavior(gfx::ELIDE_HEAD);

  auto copy = std::make_unique<views::ImageButton>(
      base::BindRepeating(&LittleBar::CopyLink, base::Unretained(this)));
  copy->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  copy->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  copy->SetTooltipText(l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_COPY_LINK));
  copy->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_COPY_LINK));
  Clickable(copy.get());
  copy_ = field_->AddChildView(std::move(copy));

  // "Open in <space> ⌘O": the space's name in its colour, as in Arc.
  auto open = std::make_unique<PillButton>(
      base::BindRepeating(
          [](base::WeakPtr<LittleWindow> window) {
            if (window) {
              window->OpenInSpace(window->default_space());
            }
          },
          window_),
      kFieldRadius, kButtonPadding);
  const std::u16string space_name = space ? space->name : std::u16string();
  size_t offset = 0;
  const std::u16string open_text = l10n_util::GetStringFUTF16(
      IDS_CROMARC_LITTLE_OPEN_IN, space_name, &offset);
  open_prefix_ = open->AddChildView(MakeLabel(
      open_text.substr(0, offset), Font(gfx::Font::Weight::NORMAL)));
  open_space_ = open->AddChildView(
      MakeLabel(space_name, Font(gfx::Font::Weight::SEMIBOLD)));
  open_suffix_ = open->AddChildView(
      MakeLabel(open_text.substr(offset + space_name.size()),
                Font(gfx::Font::Weight::NORMAL)));
  auto key = MakeLabel(
      ui::Accelerator(ui::VKEY_O, ui::EF_COMMAND_DOWN).GetShortcutText(),
      Font(gfx::Font::Weight::MEDIUM, -2));
  key->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(1, 5)));
  key->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 6, 0, 0));
  open_key_ = open->AddChildView(std::move(key));
  for (views::View* child : open->children()) {
    child->SetCanProcessEventsWithinSubtree(false);
  }
  open->GetViewAccessibility().SetName(open_text);
  Clickable(open.get());
  open_ = AddChildView(std::move(open));

  auto spaces = std::make_unique<views::ImageButton>(
      base::BindRepeating(&LittleBar::ShowSpacesMenu, base::Unretained(this)));
  spaces->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  spaces->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  spaces->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_OPEN_IN_ANOTHER));
  spaces->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_OPEN_IN_ANOTHER));
  Clickable(spaces.get());
  spaces_ = AddChildView(std::move(spaces));

  Observe(window->page());
  UpdateDomain();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kLogLittleSwitch)) {
    LOG(ERROR) << "cromarc little bar: space=" << space_name
               << " open=" << open_text << " height=" << kLittleBarHeight;
  }
}

LittleBar::~LittleBar() = default;

void LittleBar::AddedToWidget() {
  // Arc: Shift+Cmd+O shows the "Open In" menu, Option+Cmd+O too.
  AddAccelerator(
      ui::Accelerator(ui::VKEY_O, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
  AddAccelerator(
      ui::Accelerator(ui::VKEY_O, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));
}

bool LittleBar::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ShowSpacesMenu();
  return true;
}

void LittleBar::OnThemeChanged() {
  views::View::OnThemeChanged();
  ApplyColors();
}

LittleBar::Colors LittleBar::ComputeColors() const {
  const ui::ColorProvider* provider = GetColorProvider();
  const SkColor base =
      provider ? provider->GetColor(ui::kColorFrameActive) : SK_ColorWHITE;
  SkColor bar = base;
  if (!theme_.colors.empty()) {
    int r = 0, g = 0, b = 0;
    for (SkColor color : theme_.colors) {
      r += SkColorGetR(color);
      g += SkColorGetG(color);
      b += SkColorGetB(color);
    }
    const int n = static_cast<int>(theme_.colors.size());
    bar = color_utils::AlphaBlend(SkColorSetRGB(r / n, g / n, b / n), base,
                                  static_cast<float>(theme_.opacity));
  }
  const bool light_text = color_utils::IsDark(bar);
  Colors colors;
  colors.bar = bar;
  colors.text = light_text ? SkColorSetA(SK_ColorWHITE, 0xEB)
                           : SkColorSetARGB(0xE0, 0x1C, 0x1C, 0x1E);
  colors.secondary = light_text ? SkColorSetA(SK_ColorWHITE, 0xB3)
                                : SkColorSetARGB(0xA6, 0x1C, 0x1C, 0x1E);
  colors.field = light_text ? SkColorSetA(SK_ColorWHITE, 0x1A)
                            : SkColorSetA(SK_ColorBLACK, 0x0F);
  colors.chip = light_text ? SkColorSetA(SK_ColorWHITE, 0x1F)
                           : SkColorSetA(SK_ColorWHITE, 0x8C);
  colors.chip_hover = light_text ? SkColorSetA(SK_ColorWHITE, 0x33)
                                 : SkColorSetA(SK_ColorWHITE, 0xCC);
  colors.button = light_text ? SkColorSetA(SK_ColorWHITE, 0x29)
                             : SkColorSetA(SK_ColorWHITE, 0xD9);
  colors.button_hover = light_text ? SkColorSetA(SK_ColorWHITE, 0x3D)
                                   : SK_ColorWHITE;
  colors.key = light_text ? SkColorSetA(SK_ColorWHITE, 0x1F)
                          : SkColorSetA(SK_ColorBLACK, 0x0F);
  // The space's name in its own colour, readable on the button.
  const SkColor accent =
      theme_.colors.empty() ? colors.text : theme_.colors.front();
  colors.space_name =
      color_utils::BlendForMinContrast(
          accent, color_utils::GetResultingPaintColor(colors.button, bar))
          .color;
  return colors;
}

void LittleBar::ApplyColors() {
  colors_ = ComputeColors();
  field_->SetBackground(
      views::CreateRoundedRectBackground(colors_.field, kFieldRadius));

  static_cast<PillButton*>(space_.get())
      ->SetFills(colors_.chip, colors_.chip_hover);
  space_label_->SetEnabledColor(colors_.text);

  domain_->SetEnabledColor(colors_.text);

  ShowCopyIcon(copied_timer_.IsRunning());

  static_cast<PillButton*>(open_.get())
      ->SetFills(colors_.button, colors_.button_hover);
  open_prefix_->SetEnabledColor(colors_.secondary);
  open_suffix_->SetEnabledColor(colors_.secondary);
  open_space_->SetEnabledColor(colors_.space_name);
  open_key_->SetEnabledColor(colors_.secondary);
  open_key_->SetBackground(
      views::CreateRoundedRectBackground(colors_.key, kKeyRadius));

  spaces_->SetImageModel(views::Button::STATE_NORMAL,
                         ui::ImageModel::FromVectorIcon(
                             vector_icons::kKeyboardArrowDownIcon,
                             colors_.secondary, kIconSize));
  SchedulePaint();
}

void LittleBar::ShowCopyIcon(bool copied) {
  copy_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          copied ? views::kCheckIcon : vector_icons::kLinkIcon,
          colors_.secondary, kIconSize));
}

void LittleBar::OnPaint(gfx::Canvas* canvas) {
  // The window's frame colour, the space's theme over it, as a space
  // window's background.
  const ui::ColorProvider* provider = GetColorProvider();
  const SkColor base =
      provider ? provider->GetColor(ui::kColorFrameActive) : SK_ColorWHITE;
  canvas->DrawColor(base);
  PaintSpaceTheme(canvas, GetLocalBounds(), theme_, color_utils::IsDark(base));
}

void LittleBar::Layout(PassKey) {
  const int field_y = (height() - kFieldHeight) / 2;
  int right = width() - kEndMargin;

  spaces_->SetBounds(right - kIconButtonSize, field_y, kIconButtonSize,
                     kFieldHeight);
  right = spaces_->x() - kGap / 2;

  const int open_width = open_->GetPreferredSize().width();
  open_->SetBounds(right - open_width, field_y, open_width, kFieldHeight);
  right = open_->x() - kGap;

  const int field_x = kTrafficLightsRoom;
  field_->SetBounds(field_x, field_y, std::max(0, right - field_x),
                    kFieldHeight);

  // In the field: the chip at the start, Copy Link at the end, the domain
  // centred in the field where it fits, else between the two.
  const int chip_width =
      std::min(space_->GetPreferredSize().width(), field_->width() / 3);
  space_->SetBounds(kFieldInset, (kFieldHeight - kChipHeight) / 2, chip_width,
                    kChipHeight);
  copy_->SetBounds(field_->width() - kFieldInset - kIconButtonSize,
                   (kFieldHeight - kIconButtonSize) / 2, kIconButtonSize,
                   kIconButtonSize);
  const int start = space_->bounds().right() + kGap;
  const int end = copy_->x() - kGap;
  const int text_width = std::min(domain_->GetPreferredSize().width(),
                                  std::max(0, end - start));
  int text_x = (field_->width() - text_width) / 2;
  text_x = std::clamp(text_x, start, std::max(start, end - text_width));
  domain_->SetBounds(text_x, 0, text_width, kFieldHeight);

  if (!logged_hits_ && width() > 0 && GetWidget() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kLogLittleSwitch)) {
    logged_hits_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LittleBar::LogHits, weak_factory_.GetWeakPtr()));
  }
}

void LittleBar::LogHits() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  // The window's own hit test (H25), at a point of each part of the bar.
  const auto hit = [widget](views::View* view, gfx::Point point) {
    views::View::ConvertPointToWidget(view, &point);
    return widget->GetNonClientComponent(point);
  };
  const auto center = [](views::View* view) {
    return view->GetLocalBounds().CenterPoint();
  };
  views::FocusManager* focus = GetFocusManager();
  const bool keys =
      focus &&
      focus->IsAcceleratorRegistered(ui::Accelerator(
          ui::VKEY_O, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)) &&
      focus->IsAcceleratorRegistered(
          ui::Accelerator(ui::VKEY_O, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));
  LOG(ERROR) << "cromarc little hits:"
             << " empty="
             << hit(this, gfx::Point(field_->bounds().right() + kGap / 2,
                                     height() / 2))
             << " field="
             << hit(field_.get(),
                    gfx::Point(space_->bounds().right() + kGap / 2,
                               kFieldHeight / 2))
             << " chip=" << hit(space_.get(), center(space_.get()))
             << " copy=" << hit(copy_.get(), center(copy_.get()))
             << " open=" << hit(open_.get(), center(open_.get()))
             << " menu=" << hit(spaces_.get(), center(spaces_.get()))
             << " page=" << hit(this, gfx::Point(width() / 2, height() + 40))
             << " keys=" << keys;
}

void LittleBar::PrimaryPageChanged(content::Page& page) {
  UpdateDomain();
}

void LittleBar::UpdateDomain() {
  domain_->SetText(window_ ? DomainOf(window_->url()) : std::u16string());
  InvalidateLayout();
}

void LittleBar::CopyLink() {
  if (!window_ || window_->url().is_empty()) {
    return;
  }
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(base::UTF8ToUTF16(window_->url().spec()));
  }
  copy_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_CROMARC_LITTLE_LINK_COPIED));
  ShowCopyIcon(true);
  copied_timer_.Start(FROM_HERE, kCopiedFor,
                      base::BindOnce(&LittleBar::ShowCopyIcon,
                                     base::Unretained(this), false));
}

void LittleBar::ShowSpacesMenu() {
  menu_spaces_.clear();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  const std::vector<SpaceController::Space> spaces =
      SpaceController::Get()->GetSpaces();
  for (const SpaceController::Space& space : spaces) {
    menu_model_->AddItem(static_cast<int>(menu_spaces_.size()),
                         LabelOf(space));
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
  menu_runner_->RunMenuAt(GetWidget(), nullptr, spaces_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kMouse);
}

void LittleBar::ShowMoveMenu() {
  menu_spaces_.clear();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (window_ && space.id == window_->default_space()) {
      continue;
    }
    menu_model_->AddItem(
        kMoveWindowCommand + static_cast<int>(menu_spaces_.size()),
        l10n_util::GetStringFUTF16(IDS_CROMARC_LITTLE_MOVE_WINDOW,
                                   LabelOf(space)));
    menu_spaces_.push_back(space.id);
  }
  if (menu_spaces_.empty()) {
    return;
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, space_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

void LittleBar::ExecuteCommand(int command_id, int event_flags) {
  if (!window_) {
    return;
  }
  if (command_id >= kMoveWindowCommand) {
    const size_t index = static_cast<size_t>(command_id - kMoveWindowCommand);
    if (index < menu_spaces_.size()) {
      window_->MoveToSpace(menu_spaces_[index]);
    }
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
