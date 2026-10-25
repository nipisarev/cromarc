// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SWIPE_TRACKER_H_
#define CROMARC_BROWSER_SPACES_SWIPE_TRACKER_H_

#include <vector>

#include "base/time/time.h"

namespace cromarc {

// Turns trackpad scroll events with phases into the swipe events Zen's space
// switching consumes (design, component 4): the same semantics as Gecko's
// widget/SwipeTracker.cpp, with Zen's threshold and velocity contribution.
// Platform-independent; the macOS event monitor feeds it.
class SwipeTracker {
 public:
  // Gecko's widget.swipe.pixel-size: finger travel for an amount of 1.0.
  static constexpr double kPixelSize = 550.0;
  // zen:spaces/ZenSpacesSwipe.mjs#L29-L30.
  static constexpr double kSuccessThreshold = 0.25;
  static constexpr double kSuccessVelocityContribution = 0.5;

  // kNone: a scroll outside any gesture, as a mouse wheel's.
  enum class Phase { kNone, kBegan, kChanged, kEnded, kCancelled };

  struct Input {
    Phase phase;
    // A momentum (inertia) event after the fingers left the trackpad.
    bool momentum;
    // Scroll deltas in pixels, positive towards the right and the bottom.
    double dx;
    double dy;
    base::TimeTicks time;
  };

  enum class EventType { kStart, kUpdate, kEnd };

  struct Event {
    EventType type;
    // kUpdate: cumulative amount, positive towards the next space.
    double amount = 0;
    // kEnd: 1 next space, -1 previous space, 0 cancelled.
    int direction = 0;
  };

  SwipeTracker();
  ~SwipeTracker();

  std::vector<Event> Process(const Input& input);

  // Whether scroll events are being taken by a swipe (and must not reach
  // the page or the sidebar): from the start until the momentum ends.
  bool consuming() const { return state_ == State::kSwiping || state_ == State::kMomentum; }

 private:
  enum class State { kIdle, kDeciding, kSwiping, kMomentum, kIgnoring };

  bool Succeeded(int direction) const;

  State state_ = State::kIdle;
  double amount_ = 0;
  double velocity_ = 0;
  base::TimeTicks last_time_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SWIPE_TRACKER_H_
