// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/window_material.h"

#import <AppKit/AppKit.h>

// zen:widget/cocoa/nsCocoaWindow-mm.patch, ZenWindowMaterialView, with Zen's
// defaults: zen.widget.macos.window-material 1 (the HUD window material) and
// zen.view.grey-out-inactive-windows on.
@interface CromarcWindowMaterialView : NSVisualEffectView
@end

@implementation CromarcWindowMaterialView

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.appearance = nil;
    self.emphasized = NO;
    self.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    self.state = NSVisualEffectStateFollowsWindowActiveState;
    self.material = NSVisualEffectMaterialHUDWindow;
  }
  return self;
}

// As Chrome's GlassFrameBackgroundView: clicks reach the views above.
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}

@end

namespace cromarc {

void InstallWindowMaterial(gfx::NativeWindow native_window) {
  NSView* content = native_window.GetNativeNSWindow().contentView;
  if (!content) {
    return;
  }
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[CromarcWindowMaterialView class]]) {
      return;
    }
  }
  CromarcWindowMaterialView* material =
      [[CromarcWindowMaterialView alloc] initWithFrame:content.bounds];
  material.wantsLayer = YES;
  material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  // Under everything, where Chrome puts its glass.
  [content addSubview:material positioned:NSWindowBelow relativeTo:nil];
}

}  // namespace cromarc
