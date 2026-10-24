// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/space_input.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/spaces/swipe_tracker.h"
#include "cromarc/browser/ui/sidebar/compact_mode.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_ui_types.h"

namespace cromarc {

namespace {

// zen:spaces/ZenSpaceManager.mjs, #setupSidebarHandlers.
constexpr base::TimeDelta kWheelCooldown = base::Milliseconds(200);

constexpr NSEventModifierFlags kModifierMask =
    NSEventModifierFlagCommand | NSEventModifierFlagOption |
    NSEventModifierFlagControl | NSEventModifierFlagShift;

struct WindowSwipe {
  SwipeTracker tracker;
  // The window whose sidebar the swipe began over.
  SessionID browser_id = SessionID::InvalidValue();
};

// Keyed by window address: an NSWindow* key would keep closed windows alive.
std::map<uintptr_t, WindowSwipe>& Swipes() {
  static base::NoDestructor<std::map<uintptr_t, WindowSwipe>> swipes;
  return *swipes;
}

SwipeTracker::Phase PhaseOf(NSEventPhase phase) {
  if (phase & NSEventPhaseBegan) {
    return SwipeTracker::Phase::kBegan;
  }
  if (phase & NSEventPhaseEnded) {
    return SwipeTracker::Phase::kEnded;
  }
  if (phase & NSEventPhaseCancelled) {
    return SwipeTracker::Phase::kCancelled;
  }
  return SwipeTracker::Phase::kChanged;
}

NSEvent* HandleKeyDown(NSEvent* event) {
  BrowserWindowInterface* browser =
      SidebarView::BrowserFor(gfx::NativeWindow(event.window));
  if (!browser) {
    return event;
  }
  SpaceController* spaces = SpaceController::Get();
  const NSEventModifierFlags flags = event.modifierFlags & kModifierMask;
  if (flags == NSEventModifierFlagControl &&
      event.charactersIgnoringModifiers.length == 1) {
    const unichar c = [event.charactersIgnoringModifiers characterAtIndex:0];
    if (c >= '1' && c <= '9') {
      spaces->RequestSwitchToIndex(browser, c - '1');
      return nil;
    }
    if (c == '0') {
      spaces->RequestSwitchToIndex(browser, 9);
      return nil;
    }
  }
  // Compact mode: Cmd+S as Arc's "hide sidebar" (the menu's Save Page As
  // stays), Ctrl+Alt+C as Zen's.
  if ((flags == NSEventModifierFlagCommand && event.keyCode == kVK_ANSI_S) ||
      (flags == (NSEventModifierFlagControl | NSEventModifierFlagOption) &&
       event.keyCode == kVK_ANSI_C)) {
    CompactMode::Toggle(browser);
    return nil;
  }
  if (flags == (NSEventModifierFlagCommand | NSEventModifierFlagOption)) {
    if (event.keyCode == kVK_LeftArrow) {
      spaces->RequestSwitchBy(browser, -1);
      return nil;
    }
    if (event.keyCode == kVK_RightArrow) {
      spaces->RequestSwitchBy(browser, 1);
      return nil;
    }
  }
  return event;
}

NSEvent* HandleScroll(NSEvent* event) {
  NSWindow* window = event.window;
  if (!window) {
    return event;
  }
  WindowSwipe& swipe =
      Swipes()[reinterpret_cast<uintptr_t>((__bridge void*)window)];
  const bool trackpad = event.phase != NSEventPhaseNone ||
                        event.momentumPhase != NSEventPhaseNone;

  if (!swipe.tracker.consuming()) {
    SidebarView* sidebar = SidebarView::FindAt(
        gfx::NativeWindow(window),
        gfx::ScreenPointFromNSPoint([NSEvent mouseLocation]));
    if (!trackpad) {
      // Ctrl + wheel over the sidebar: one space per notch.
      if (sidebar && (event.modifierFlags & NSEventModifierFlagControl) &&
          event.scrollingDeltaY != 0) {
        static base::TimeTicks last;
        if (base::TimeTicks::Now() - last > kWheelCooldown) {
          last = base::TimeTicks::Now();
          SpaceController::Get()->RequestSwitchBy(
              sidebar->browser(), event.scrollingDeltaY > 0 ? -1 : 1);
        }
        return nil;
      }
      return event;
    }
    // Only gestures that begin over the sidebar can switch spaces.
    if (event.phase == NSEventPhaseBegan) {
      swipe.browser_id = sidebar ? sidebar->browser()->GetSessionID()
                                 : SessionID::InvalidValue();
    }
  }
  BrowserWindowInterface* browser =
      BrowserWindowInterface::FromSessionID(swipe.browser_id);
  if (!browser) {
    return event;
  }

  // Finger travel, positive to the right, whatever the scrolling direction
  // setting.
  const double sign = event.isDirectionInvertedFromDevice ? 1 : -1;
  const SwipeTracker::Input input{
      event.momentumPhase != NSEventPhaseNone
          ? PhaseOf(event.momentumPhase)
          : PhaseOf(event.phase),
      event.momentumPhase != NSEventPhaseNone,
      sign * event.scrollingDeltaX,
      sign * event.scrollingDeltaY,
      base::TimeTicks::Now(),
  };
  const bool was_consuming = swipe.tracker.consuming();
  for (const SwipeTracker::Event& e : swipe.tracker.Process(input)) {
    SpaceController::Get()->OnSwipe(browser, e);
  }
  if (!swipe.tracker.consuming() && !was_consuming) {
    return event;
  }
  return nil;
}

NSEvent* HandleMouseMoved(NSEvent* event) {
  if (SidebarView* sidebar =
          SidebarView::ForWindow(gfx::NativeWindow(event.window))) {
    sidebar->compact_mode()->OnMouseMoved(
        gfx::ScreenPointFromNSPoint([NSEvent mouseLocation]));
  }
  return event;
}

}  // namespace

void InstallSpaceInputMonitor() {
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                        handler:^NSEvent*(NSEvent* event) {
                                          return HandleKeyDown(event);
                                        }];
  [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                        handler:^NSEvent*(NSEvent* event) {
                                          return HandleScroll(event);
                                        }];
  // Compact mode's left edge.
  [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved
                                        handler:^NSEvent*(NSEvent* event) {
                                          return HandleMouseMoved(event);
                                        }];
}

}  // namespace cromarc
