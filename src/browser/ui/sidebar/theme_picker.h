// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "cromarc/browser/ports/vertical_strip.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class WebView;
class WidgetDelegate;
}  // namespace views

namespace cromarc {

class ThemePickerPageHandler;
class Windows;

// Zen's theme picker (zen:spaces/ZenGradientGenerator.mjs,
// zen-panels/theme-picker.inc) for one browser window: a panel beside the
// sidebar that hosts chrome://cromarc-theme-picker.top-chrome and edits one
// space's theme. Hidden rather than destroyed between uses, destroyed with
// its window.
class ThemePicker : public views::WidgetObserver {
 public:
  static ThemePicker* GetOrCreate(Windows& windows,
                                  BrowserWindowInterface* browser);
  static ThemePicker* Get(BrowserWindowInterface* browser);

  ThemePicker(Windows& windows, BrowserWindowInterface* browser);
  ThemePicker(const ThemePicker&) = delete;
  ThemePicker& operator=(const ThemePicker&) = delete;
  ~ThemePicker() override;

  void Show(const std::string& space_id);
  // As Zen's panel when it hides: the theme previewed is saved.
  void Hide();
  bool IsShowing() const;
  const std::string& space_id() const { return space_id_; }

  // The page's handler; it is told the space on every Show().
  void SetPageHandler(ThemePickerPageHandler* handler);
  void SetContentHeight(int height);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void CreateWidget();
  gfx::Rect Bounds() const;

  const raw_ref<Windows> windows_;
  raw_ptr<BrowserWindowInterface> browser_;
  std::string space_id_;
  int content_height_;
  // Keeps the compact sidebar out while the panel is open beside it.
  std::unique_ptr<VerticalStrip::Hold> strip_hold_;
  // Outlives `widget_`.
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<ThemePickerPageHandler> page_handler_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::CallbackListSubscription browser_did_close_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_H_
