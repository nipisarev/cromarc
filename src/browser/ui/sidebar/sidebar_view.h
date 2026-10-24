// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;

namespace cromarc {

// Hosts chrome://cromarc-sidebar.top-chrome for one browser window. Fills the
// space the native tab list had in the vertical tab strip region.
class SidebarView : public views::WebView {
  METADATA_HEADER(SidebarView, views::WebView)

 public:
  explicit SidebarView(BrowserWindowInterface* browser);
  SidebarView(const SidebarView&) = delete;
  SidebarView& operator=(const SidebarView&) = delete;
  ~SidebarView() override;

  // The sidebar of `window` under `screen_point`, if any (for the space
  // switching input monitor).
  static SidebarView* FindAt(gfx::NativeWindow window,
                             const gfx::Point& screen_point);
  // The browser window `window` belongs to, if it has a sidebar.
  static BrowserWindowInterface* BrowserFor(gfx::NativeWindow window);

  BrowserWindowInterface* browser() const { return browser_; }

  // content::WebContentsDelegate (through views::WebView):
  // Browser shortcuts (Cmd+T, Cmd+W, ...) keep working while the sidebar has
  // focus.
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
