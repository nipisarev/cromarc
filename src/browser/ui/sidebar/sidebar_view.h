// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
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

  // content::WebContentsDelegate (through views::WebView):
  // Browser shortcuts (Cmd+T, Cmd+W, ...) keep working while the sidebar has
  // focus.
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
