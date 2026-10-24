// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_LIFECYCLE_POLICY_H_
#define CROMARC_BROWSER_LIFECYCLE_POLICY_H_

#include "base/time/time.h"

namespace cromarc {

// After this much idle time a Today tab goes to the archive and a pinned
// item's live tab falls asleep (design, component 3).
inline constexpr base::TimeDelta kIdleLimit = base::Hours(12);
// No system input for this long counts as the user being away: then even the
// active tab of a visible window idles, counting from the last input.
inline constexpr base::TimeDelta kAwayAfter = base::Minutes(15);
// The sidebar shows the time left only within this window.
inline constexpr base::TimeDelta kShowTimeLeftWithin = base::Hours(1);

// The thresholds above; tests and --cromarc-idle-limit shorten them.
struct Limits {
  base::TimeDelta idle = kIdleLimit;
  base::TimeDelta away = kAwayAfter;
  base::TimeDelta show_time_left_within = kShowTimeLeftWithin;
};

// What the lifecycle engine knows about one tab at one moment.
struct TabFacts {
  // The tab belongs to a pinned item (else it is a Today tab).
  bool pinned_item = false;
  // Last moment the tab was the active tab of a visible window.
  base::Time last_active;
  bool active_in_visible_window = false;
  // How long there has been no system input (0 while the user is here).
  base::TimeDelta user_away_for;
  // Playing sound or capturing camera, microphone or screen.
  bool paused = false;
  // The page has a beforeunload handler and the user typed into it: closing
  // could lose input.
  bool unsaved_input = false;
  // The user chose "Keep" for a deferred tab.
  bool kept_by_user = false;
};

enum class Decision {
  kKeep,
  // Today tab: record in the archive, close.
  kArchive,
  // Pinned item: close the live tab, the item sleeps.
  kSleep,
  // Would close, but could lose input: show it under "Needs attention".
  kDefer,
};

Decision Decide(const TabFacts& facts,
                base::Time now,
                const Limits& limits = Limits());

// Time until Decide() archives or puts the tab to sleep, when that is within
// `limits.show_time_left_within`; zero otherwise.
base::TimeDelta TimeToArchive(const TabFacts& facts,
                              base::Time now,
                              const Limits& limits = Limits());

}  // namespace cromarc

#endif  // CROMARC_BROWSER_LIFECYCLE_POLICY_H_
