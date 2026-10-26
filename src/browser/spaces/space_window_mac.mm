// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_window.h"

#import <AppKit/AppKit.h>

namespace cromarc {

void SetWindowFaint(gfx::NativeWindow native_window, bool faint) {
  NSWindow* window = native_window.GetNativeNSWindow();
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
