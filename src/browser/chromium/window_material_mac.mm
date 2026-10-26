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

// The glass of a native popover on macOS 26 and later, under a panel window.
API_AVAILABLE(macos(26.0))
@interface CromarcPanelGlassView : NSGlassEffectView
@end

@implementation CromarcPanelGlassView

- (NSView*)hitTest:(NSPoint)point {
  return nil;
}

@end

namespace {

// NSPopover's frame on macOS 26 and later (seen in its view tree): an
// NSGlassEffectView with corner radius 20, of the private variant 20, which is
// lighter and softer than plain glass. Without the variant the glass is the
// plain one.
constexpr int kPopoverGlassCornerRadius = 20;
constexpr int kPopoverGlassVariant = 20;
// Before macOS 26, NSPopover's material.
constexpr int kPopoverMaterialCornerRadius = 12;

bool HasMaterial(NSView* content) {
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[CromarcWindowMaterialView class]]) {
      return true;
    }
    if (@available(macOS 26.0, *)) {
      if ([view isKindOfClass:[CromarcPanelGlassView class]]) {
        return true;
      }
    }
  }
  return false;
}

// Under everything, where Chrome puts its glass.
void AddUnder(NSView* content, NSView* material) {
  material.wantsLayer = YES;
  material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [content addSubview:material positioned:NSWindowBelow relativeTo:nil];
}

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

NSView* ContentViewWithoutMaterial(gfx::NativeWindow native_window) {
  NSView* content = native_window.GetNativeNSWindow().contentView;
  return content && !HasMaterial(content) ? content : nil;
}

CromarcWindowMaterialView* AddMaterial(NSView* content) {
  CromarcWindowMaterialView* material =
      [[CromarcWindowMaterialView alloc] initWithFrame:content.bounds];
  AddUnder(content, material);
  return material;
}

}  // namespace

namespace cromarc {

void InstallWindowMaterial(gfx::NativeWindow window) {
  NSView* content = ContentViewWithoutMaterial(window);
  if (!content) {
    return;
  }
  // Zen's defaults: zen.widget.macos.window-material 1 (the HUD window
  // material), zen.view.grey-out-inactive-windows on.
  CromarcWindowMaterialView* material = AddMaterial(content);
  material.material = NSVisualEffectMaterialHUDWindow;
  material.state = NSVisualEffectStateFollowsWindowActiveState;
}

int PanelCornerRadius() {
  if (@available(macOS 26.0, *)) {
    return kPopoverGlassCornerRadius;
  }
  return kPopoverMaterialCornerRadius;
}

void InstallPanelMaterial(gfx::NativeWindow window) {
  NSView* content = ContentViewWithoutMaterial(window);
  if (!content) {
    return;
  }
  if (@available(macOS 26.0, *)) {
    CromarcPanelGlassView* glass =
        [[CromarcPanelGlassView alloc] initWithFrame:content.bounds];
    glass.cornerRadius = kPopoverGlassCornerRadius;
    if ([glass respondsToSelector:NSSelectorFromString(@"set_variant:")]) {
      [glass setValue:@(kPopoverGlassVariant) forKey:@"_variant"];
    }
    AddUnder(content, glass);
    return;
  }
  CromarcWindowMaterialView* material = AddMaterial(content);
  material.material = NSVisualEffectMaterialPopover;
  material.state = NSVisualEffectStateActive;
  material.maskImage = RoundedMask(kPopoverMaterialCornerRadius);
}

}  // namespace cromarc
