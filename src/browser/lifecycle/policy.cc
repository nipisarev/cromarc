// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/policy.h"

namespace cromarc {

namespace {

// Idle time as the design counts it; zero when the tab is in use.
base::TimeDelta IdleFor(const TabFacts& facts,
                        base::Time now,
                        const Limits& limits) {
  if (facts.paused) {
    return base::TimeDelta();
  }
  if (facts.active_in_visible_window) {
    return facts.user_away_for >= limits.away ? facts.user_away_for
                                              : base::TimeDelta();
  }
  return now - facts.last_active;
}

}  // namespace

Decision Decide(const TabFacts& facts, base::Time now, const Limits& limits) {
  if (IdleFor(facts, now, limits) <= limits.idle || facts.kept_by_user) {
    return Decision::kKeep;
  }
  if (facts.unsaved_input) {
    return Decision::kDefer;
  }
  return facts.pinned_item ? Decision::kSleep : Decision::kArchive;
}

base::TimeDelta TimeToArchive(const TabFacts& facts,
                              base::Time now,
                              const Limits& limits) {
  const base::TimeDelta idle = IdleFor(facts, now, limits);
  if (idle.is_zero() || idle > limits.idle ||
      limits.idle - idle > limits.show_time_left_within) {
    return base::TimeDelta();
  }
  return limits.idle - idle;
}

}  // namespace cromarc
