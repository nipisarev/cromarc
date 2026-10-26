// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WidgetDelegate;
}  // namespace views

namespace cromarc {

class GlanceView;
class Tabs;
class Windows;

// Glance (design, component 7 and UI section 8), after Zen's ZenGlanceManager:
// Option+click on a link previews it in a card over the page; the card grows
// from the click. Esc, a click on the dimmed page or ✕ closes it; ⧉ makes it
// a tab in Today.
class Glance : public content::WebContentsDelegate,
               public views::WidgetObserver {
 public:
  // Replaces the window's open Glance, if any.
  static void Open(Tabs& tabs,
                   Windows& windows,
                   BrowserWindowInterface* browser,
                   const GURL& url,
                   const gfx::Point& screen_point);
  static Glance* Get(BrowserWindowInterface* browser);

  Glance(const Glance&) = delete;
  Glance& operator=(const Glance&) = delete;
  ~Glance() override;

  void Close();
  void OpenAsTab();

  content::WebContents* contents() { return contents_.get(); }

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;

  // views::WidgetObserver, for the browser window:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  Glance(Tabs& tabs,
         Windows& windows,
         BrowserWindowInterface* browser,
         const GURL& url,
         const gfx::Point& screen_point);

  // The page area of the window, in screen coordinates.
  gfx::Rect PageBounds() const;
  // Destroys this Glance.
  void Destroy();

  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
  const raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<GlanceView> view_ = nullptr;
  bool closing_ = false;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{this};
  base::CallbackListSubscription browser_did_close_;
  // A switch to another tab closes it.
  base::CallbackListSubscription active_tab_changed_;
  base::WeakPtrFactory<Glance> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_H_
