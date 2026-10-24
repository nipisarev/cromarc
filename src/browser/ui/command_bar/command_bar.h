// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_H_
#define CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class WebView;
class WidgetDelegate;
}  // namespace views

namespace cromarc {

class CommandBarPageHandler;

// The command bar of one browser window (design, component 6): a floating
// panel over the window that hosts chrome://cromarc-command-bar.top-chrome.
// Created on first use, hidden rather than destroyed between uses so it opens
// at once, destroyed with its window.
class CommandBar : public views::WidgetObserver {
 public:
  static CommandBar* GetOrCreate(BrowserWindowInterface* browser);
  static CommandBar* Get(BrowserWindowInterface* browser);

  explicit CommandBar(BrowserWindowInterface* browser);
  CommandBar(const CommandBar&) = delete;
  CommandBar& operator=(const CommandBar&) = delete;
  ~CommandBar() override;

  void Show(bool edit_current_url);
  void Hide();

  bool edit_current_url() const { return edit_current_url_; }
  // What the page starts from: the current URL for Cmd+L, "" for Cmd+T.
  std::u16string InitialText() const;

  // The page's handler; it gets OnShown() on every Show().
  void SetPageHandler(CommandBarPageHandler* handler);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void CreateWidget();
  gfx::Rect Bounds() const;

  raw_ptr<BrowserWindowInterface> browser_;
  bool edit_current_url_ = false;
  // Outlives `widget_`.
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<CommandBarPageHandler> page_handler_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::CallbackListSubscription browser_did_close_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_H_
