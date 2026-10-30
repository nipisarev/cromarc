// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_PRESENCE_H_
#define CROMARC_BROWSER_PORTS_FAKE_PRESENCE_H_

#include "cromarc/browser/ports/presence.h"

namespace cromarc {

class FakePresence : public Presence {
 public:
  FakePresence();
  ~FakePresence() override;

  // Presence:
  base::TimeDelta IdleFor() override;
  bool IsScreenLocked() override;

  base::TimeDelta idle_for;
  bool screen_locked = false;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_PRESENCE_H_
