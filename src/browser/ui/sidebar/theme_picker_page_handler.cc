// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/theme_picker_page_handler.h"

#include <utility>

#include "build/build_config.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/sidebar/haptic_feedback.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "cromarc/browser/ui/sidebar/theme_picker.h"

namespace cromarc {

ThemePickerPageHandler::ThemePickerPageHandler(
    mojo::PendingReceiver<theme_picker::mojom::PageHandler> receiver,
    mojo::PendingRemote<theme_picker::mojom::Page> page,
    BrowserWindowInterface* browser,
    Windows& windows)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser),
      windows_(windows) {
  if (ThemePicker* picker = ThemePicker::Get(browser_)) {
    picker->SetPageHandler(this);
  }
}

ThemePickerPageHandler::~ThemePickerPageHandler() {
  if (ThemePicker* picker = ThemePicker::Get(browser_)) {
    picker->SetPageHandler(nullptr);
  }
}

void ThemePickerPageHandler::NotifyOpened(const std::string& space_id) {
  space_id_ = space_id;
  edited_.reset();
  std::string theme;
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (space.id == space_id) {
      theme = space.theme;
    }
  }
  page_->Open(space_id, theme);
}

void ThemePickerPageHandler::Commit() {
  if (edited_) {
    SpaceController::Get()->SetSpaceTheme(space_id_, *edited_);
    edited_.reset();
  }
  SetWindowPreview(std::nullopt);
}

void ThemePickerPageHandler::Preview(const std::string& theme) {
  edited_ = theme;
  if (EditsOwnSpace()) {
    SetWindowPreview(theme);
  }
}

void ThemePickerPageHandler::PlayHapticFeedback() {
#if BUILDFLAG(IS_MAC)
  PerformHapticFeedback();
#endif
}

void ThemePickerPageHandler::Close() {
  if (ThemePicker* picker = ThemePicker::Get(browser_)) {
    picker->Hide();
  }
}

void ThemePickerPageHandler::SetHeight(int32_t height) {
  if (ThemePicker* picker = ThemePicker::Get(browser_); picker && height > 0) {
    picker->SetContentHeight(height);
  }
}

bool ThemePickerPageHandler::EditsOwnSpace() const {
  return space_id_ == SpaceController::IdOf(windows_->ProfileOf(browser_));
}

void ThemePickerPageHandler::SetWindowPreview(
    std::optional<std::string> theme) {
  if (SidebarView* sidebar = SidebarView::ForBrowser(browser_)) {
    sidebar->SetThemePreview(std::move(theme));
  }
}

}  // namespace cromarc
