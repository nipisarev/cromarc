// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <AppKit/AppKit.h>

#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "cromarc/browser/chromium/own_titlebar.h"
#include "cromarc/hooks/hooks.h"

namespace cromarc::hooks {

void AdjustWindowParams(BrowserView* browser_view,
                        remote_cocoa::mojom::CreateWindowParams* params) {
  if (!cromarc::OwnTitlebarHeight(browser_view)) {
    return;
  }
  // As Chrome's own windows with a tab strip: the content runs under a clear
  // title bar, whose height (H24) centres the traffic lights in Cromarc's.
  params->window_class = remote_cocoa::mojom::WindowClass::kBrowser;
  params->style_mask |= NSWindowStyleMaskFullSizeContentView;
  params->titlebar_appears_transparent = true;
  params->window_title_hidden = true;
}

}  // namespace cromarc::hooks
