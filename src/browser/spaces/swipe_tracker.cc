// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/swipe_tracker.h"

#include <algorithm>
#include <cmath>

namespace cromarc {

namespace {

// Gecko's widget.swipe.velocity-twitch-tolerance: velocity against the swipe
// that still counts as holding still.
constexpr double kVelocityTwitchTolerance = 0.0000001;

}  // namespace

SwipeTracker::SwipeTracker() = default;
SwipeTracker::~SwipeTracker() = default;

bool SwipeTracker::Succeeded(int direction) const {
  if (velocity_ * direction < -kVelocityTwitchTolerance) {
    return false;
  }
  return (amount_ + velocity_ * kSuccessVelocityContribution) * direction >=
         kSuccessThreshold;
}

std::vector<SwipeTracker::Event> SwipeTracker::Process(const Input& input) {
  std::vector<Event> events;

  if (input.momentum) {
    if (state_ == State::kMomentum &&
        (input.phase == Phase::kEnded || input.phase == Phase::kCancelled)) {
      state_ = State::kIdle;
    }
    return events;
  }

  switch (input.phase) {
    // A wheel turning: no gesture is going on, whatever this still waited
    // for (a swipe's momentum may never come); a swipe cut short is
    // cancelled.
    case Phase::kNone:
      if (state_ == State::kSwiping) {
        events.push_back({EventType::kEnd, amount_, 0});
      }
      state_ = State::kIdle;
      return events;

    case Phase::kBegan:
      state_ = State::kDeciding;
      amount_ = 0;
      velocity_ = 0;
      last_time_ = input.time;
      return events;

    case Phase::kChanged: {
      if (state_ == State::kDeciding) {
        if (input.dx == 0 && input.dy == 0) {
          return events;
        }
        // The first movement picks the axis; only horizontal swipes switch.
        if (std::abs(input.dx) <= std::abs(input.dy)) {
          state_ = State::kIgnoring;
          return events;
        }
        state_ = State::kSwiping;
        events.push_back({EventType::kStart});
      }
      if (state_ != State::kSwiping) {
        return events;
      }
      // Fingers moving left move the content left: towards the next space.
      const double delta = -input.dx / kPixelSize;
      amount_ += delta;
      const double elapsed =
          std::max(0.008, (input.time - last_time_).InSecondsF());
      velocity_ = delta / elapsed;
      last_time_ = input.time;
      events.push_back({EventType::kUpdate, amount_});
      return events;
    }

    case Phase::kEnded:
    case Phase::kCancelled: {
      if (state_ != State::kSwiping) {
        state_ = State::kIdle;
        return events;
      }
      int direction = 0;
      if (input.phase == Phase::kEnded) {
        const int candidate = amount_ + velocity_ >= 0 ? 1 : -1;
        if (Succeeded(candidate)) {
          direction = candidate;
        }
      }
      events.push_back({EventType::kEnd, amount_, direction});
      state_ = State::kMomentum;
      return events;
    }
  }
  return events;
}

}  // namespace cromarc
