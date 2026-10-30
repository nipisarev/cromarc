// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_presence.h"

namespace cromarc {

FakePresence::FakePresence() = default;
FakePresence::~FakePresence() = default;

base::TimeDelta FakePresence::IdleFor() {
  return idle_for;
}

bool FakePresence::IsScreenLocked() {
  return screen_locked;
}

}  // namespace cromarc
