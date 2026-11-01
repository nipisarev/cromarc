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
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "cromarc/browser/ui/sidebar/space_theme_painter.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"

namespace views {
class Button;
class ImageButton;
class Label;
class MenuRunner;
}  // namespace views

namespace cromarc {

class LittleWindow;

// A Little window's title bar, as Arc's Little Arc has it (Arc's own
// screenshots are the reference: Arc has no code to port): one bar in the
// space's colour with the traffic lights in it; a field with the space, the
// page's domain and Copy Link; "Open in <space> ⌘O"; and a menu of the
// spaces. Its empty space moves the window.
class LittleBar : public views::View,
                  public ui::SimpleMenuModel::Delegate,
                  public content::WebContentsObserver {
  METADATA_HEADER(LittleBar, views::View)

 public:
  explicit LittleBar(LittleWindow* window);
  LittleBar(const LittleBar&) = delete;
  LittleBar& operator=(const LittleBar&) = delete;
  ~LittleBar() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  void AddedToWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout(PassKey) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  // The colours of everything on the bar, from the space's theme.
  struct Colors {
    SkColor bar;
    SkColor text;
    SkColor secondary;
    SkColor field;
    SkColor chip;
    SkColor chip_hover;
    SkColor button;
    SkColor button_hover;
    SkColor key;
    SkColor space_name;
  };

  void ShowSpacesMenu();
  void ShowMoveMenu();
  void CopyLink();
  void ShowCopyIcon(bool copied);
  void UpdateDomain();
  void ApplyColors();
  // For tests (--cromarc-test-log-little): the window's hit test on the bar.
  void LogHits();
  Colors ComputeColors() const;

  // The window outlives its bar only until it closes.
  base::WeakPtr<LittleWindow> window_;
  SpaceTheme theme_;
  Colors colors_{};

  raw_ptr<views::View> field_ = nullptr;
  raw_ptr<views::Button> space_ = nullptr;
  raw_ptr<views::Label> space_label_ = nullptr;
  raw_ptr<views::Label> domain_ = nullptr;
  raw_ptr<views::ImageButton> copy_ = nullptr;
  raw_ptr<views::Button> open_ = nullptr;
  raw_ptr<views::Label> open_prefix_ = nullptr;
  raw_ptr<views::Label> open_space_ = nullptr;
  raw_ptr<views::Label> open_suffix_ = nullptr;
  raw_ptr<views::Label> open_key_ = nullptr;
  raw_ptr<views::ImageButton> spaces_ = nullptr;

  base::OneShotTimer copied_timer_;
  std::vector<std::string> menu_spaces_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool logged_hits_ = false;
  base::WeakPtrFactory<LittleBar> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_LITTLE_LITTLE_BAR_H_
