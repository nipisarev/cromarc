// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_SIDEBAR_EXTENSIONS_H_
#define CROMARC_BROWSER_CHROMIUM_SIDEBAR_EXTENSIONS_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class View;
class Widget;
class WidgetDelegate;
}  // namespace views

namespace cromarc {

// The window's extension buttons where Zen's single toolbar puts them, at the
// end of the address field (ZenVerticalTabsManager.appendCustomizableItem:
// before #page-action-buttons). Chromium's extensions container leaves the
// hidden toolbar for a child widget there, so its popups anchor to it, and
// goes back before the window comes down.
class SidebarExtensions : public views::WidgetObserver,
                          public views::ViewObserver {
 public:
  explicit SidebarExtensions(BrowserWindowInterface* browser);
  SidebarExtensions(const SidebarExtensions&) = delete;
  SidebarExtensions& operator=(const SidebarExtensions&) = delete;
  ~SidebarExtensions() override;

  // Shows the buttons at `bounds_in_screen`; empty hides them.
  void Place(const gfx::Rect& bounds_in_screen);
  int PreferredWidth() const;
  base::CallbackListSubscription OnWidthChanged(base::RepeatingClosure callback);

  // views::WidgetObserver, for the browser window:
  void OnWidgetDestroying(views::Widget* widget) override;
  // views::ViewObserver, for the container:
  void OnViewPreferredSizeChanged(views::View* view) override;

 private:
  views::View* Container() const;
  void Take();
  void GiveBack();

  const raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  // Taken from the toolbar, at this index in it.
  raw_ptr<views::View> taken_ = nullptr;
  size_t toolbar_index_ = 0;
  base::RepeatingClosureList width_changed_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      window_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver>
      container_observation_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_SIDEBAR_EXTENSIONS_H_
