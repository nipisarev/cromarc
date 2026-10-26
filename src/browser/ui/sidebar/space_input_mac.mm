// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/space_input.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
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

// One trackpad, one gesture at a time, whichever window gets its events: a
// swipe's momentum goes on in the window the swipe brought up.
struct Swipe {
  SwipeTracker tracker;
  // The window whose sidebar the swipe began over.
  SessionID browser_id = SessionID::InvalidValue();
};

Swipe& CurrentSwipe() {
  static base::NoDestructor<Swipe> swipe;
  return *swipe;
}

SwipeTracker::Phase PhaseOf(NSEventPhase phase) {
  if (phase == NSEventPhaseNone) {
    return SwipeTracker::Phase::kNone;
  }
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
    SidebarView::ToggleCompactMode(browser);
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
  Swipe& swipe = CurrentSwipe();
  const bool trackpad = event.phase != NSEventPhaseNone ||
                        event.momentumPhase != NSEventPhaseNone;
  SidebarView* sidebar = SidebarView::FindAt(
      gfx::NativeWindow(window),
      gfx::ScreenPointFromNSPoint([NSEvent mouseLocation]));

  if (!trackpad) {
    // A swipe left waiting for momentum (which went to the window it brought
    // up, or never came) took the wheel's scrolling until the next trackpad
    // gesture.
    const std::vector<SwipeTracker::Event> ended = swipe.tracker.Process(
        {SwipeTracker::Phase::kNone, false, 0, 0, base::TimeTicks::Now()});
    if (BrowserWindowInterface* browser =
            BrowserWindowInterface::FromSessionID(swipe.browser_id)) {
      for (const SwipeTracker::Event& e : ended) {
        SpaceController::Get()->OnSwipe(browser, e);
      }
    }
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
  // Only gestures that begin over the sidebar can switch spaces, and not in
  // native fullscreen: there the switch is macOS's Space animation, which a
  // swipe cannot drive. Fingers down again start a new gesture, wherever
  // the last one's momentum went.
  if (event.phase == NSEventPhaseBegan) {
    const bool fullscreen =
        (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
    swipe.browser_id = sidebar && !fullscreen
                           ? sidebar->browser()->GetSessionID()
                           : SessionID::InvalidValue();
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

base::ScopedClosureRunner WatchMouseEverywhere(
    base::RepeatingCallback<void(const gfx::Point&)> on_move) {
  const NSEventMask mask = NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged |
                           NSEventMaskRightMouseDragged |
                           NSEventMaskOtherMouseDragged;
  id local = [NSEvent
      addLocalMonitorForEventsMatchingMask:mask
                                   handler:^NSEvent*(NSEvent* event) {
                                     on_move.Run(gfx::ScreenPointFromNSPoint(
                                         [NSEvent mouseLocation]));
                                     return event;
                                   }];
  // Other apps' windows: moves there never reach a local monitor.
  id global = [NSEvent
      addGlobalMonitorForEventsMatchingMask:mask
                                    handler:^(NSEvent* event) {
                                      on_move.Run(gfx::ScreenPointFromNSPoint(
                                          [NSEvent mouseLocation]));
                                    }];
  // Removes both as the runner runs.
  struct Monitors {
    ~Monitors() {
      if (local) {
        [NSEvent removeMonitor:local];
      }
      if (global) {
        [NSEvent removeMonitor:global];
      }
    }
    id __strong local;
    id __strong global;
  };
  auto monitors = std::make_unique<Monitors>();
  monitors->local = local;
  monitors->global = global;
  return base::ScopedClosureRunner(base::BindOnce(
      [](std::unique_ptr<Monitors>) {}, std::move(monitors)));
}

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
