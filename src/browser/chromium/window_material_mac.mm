// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/window_material.h"

#import <AppKit/AppKit.h>

// zen:widget/cocoa/nsCocoaWindow-mm.patch, ZenWindowMaterialView: blends what
// is behind the window.
@interface CromarcWindowMaterialView : NSVisualEffectView
@end

@implementation CromarcWindowMaterialView

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.appearance = nil;
    self.emphasized = NO;
    self.blendingMode = NSVisualEffectBlendingModeBehindWindow;
  }
  return self;
}

// As Chrome's GlassFrameBackgroundView: clicks reach the views above.
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}

@end

namespace {

// A stretchable rounded rectangle, the mask of a material with round corners.
NSImage* RoundedMask(CGFloat radius) {
  const CGFloat edge = 2 * radius + 1;
  NSImage* mask = [NSImage
       imageWithSize:NSMakeSize(edge, edge)
             flipped:NO
      drawingHandler:^BOOL(NSRect rect) {
        [NSColor.blackColor setFill];
        [[NSBezierPath bezierPathWithRoundedRect:rect
                                         xRadius:radius
                                         yRadius:radius] fill];
        return YES;
      }];
  mask.capInsets = NSEdgeInsetsMake(radius, radius, radius, radius);
  mask.resizingMode = NSImageResizingModeStretch;
  return mask;
}

CromarcWindowMaterialView* Install(gfx::NativeWindow native_window) {
  NSView* content = native_window.GetNativeNSWindow().contentView;
  if (!content) {
    return nil;
  }
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[CromarcWindowMaterialView class]]) {
      return nil;
    }
  }
  CromarcWindowMaterialView* material =
      [[CromarcWindowMaterialView alloc] initWithFrame:content.bounds];
  material.wantsLayer = YES;
  material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  // Under everything, where Chrome puts its glass.
  [content addSubview:material positioned:NSWindowBelow relativeTo:nil];
  return material;
}

}  // namespace

namespace cromarc {

void InstallWindowMaterial(gfx::NativeWindow window) {
  // Zen's defaults: zen.widget.macos.window-material 1 (the HUD window
  // material), zen.view.grey-out-inactive-windows on.
  if (CromarcWindowMaterialView* material = Install(window)) {
    material.material = NSVisualEffectMaterialHUDWindow;
    material.state = NSVisualEffectStateFollowsWindowActiveState;
  }
}

void InstallPanelMaterial(gfx::NativeWindow window, int corner_radius) {
  if (CromarcWindowMaterialView* material = Install(window)) {
    material.material = NSVisualEffectMaterialPopover;
    material.state = NSVisualEffectStateActive;
    material.maskImage = RoundedMask(corner_radius);
  }
}

}  // namespace cromarc
