// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/system_accent.h"

#import <AppKit/AppKit.h>

#include "base/numerics/safe_conversions.h"

namespace cromarc {

SkColor SystemAccentColor() {
  NSColor* accent = [NSColor.controlAccentColor
      colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
  CGFloat r = 0, g = 0, b = 0, a = 1;
  [accent getRed:&r green:&g blue:&b alpha:&a];
  return SkColorSetARGB(base::ClampRound(a * 255), base::ClampRound(r * 255),
                        base::ClampRound(g * 255), base::ClampRound(b * 255));
}

}  // namespace cromarc
