// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/presence_adapter.h"

#include "base/command_line.h"
#include "ui/base/idle/idle.h"

namespace cromarc {

namespace {

// For tests: the user is away, whatever the Mac's idle time (headless
// windows do not tell which one is in front).
constexpr char kUserAwaySwitch[] = "cromarc-user-away";

}  // namespace

PresenceAdapter::PresenceAdapter() = default;
PresenceAdapter::~PresenceAdapter() = default;

base::TimeDelta PresenceAdapter::IdleFor() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kUserAwaySwitch)) {
    return base::Days(1);
  }
  return base::Seconds(ui::CalculateIdleTime());
}

bool PresenceAdapter::IsScreenLocked() {
  return ui::CheckIdleStateIsLocked();
}

}  // namespace cromarc
