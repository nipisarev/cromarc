// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_window.h"

#import <AppKit/AppKit.h>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"

namespace cromarc {

void SetWindowFaint(BrowserWindowInterface* browser, bool faint) {
  NSWindow* window =
      browser->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  if (!window) {
    return;
  }
  // Not 0: macOS may count a fully transparent window as occluded, and
  // Chrome then stops its pages painting.
  window.alphaValue = faint ? 0.01 : 1.0;
  window.ignoresMouseEvents = faint;
  if (faint) {
    // No zoom-in as it comes forward.
    window.animationBehavior = NSWindowAnimationBehaviorNone;
  }
}

}  // namespace cromarc
