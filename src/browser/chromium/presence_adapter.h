// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_PRESENCE_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_PRESENCE_ADAPTER_H_

#include "cromarc/browser/ports/presence.h"

namespace cromarc {

// Presence on the system's idle time and screen lock.
class PresenceAdapter : public Presence {
 public:
  PresenceAdapter();
  PresenceAdapter(const PresenceAdapter&) = delete;
  PresenceAdapter& operator=(const PresenceAdapter&) = delete;
  ~PresenceAdapter() override;

  // Presence:
  base::TimeDelta IdleFor() override;
  bool IsScreenLocked() override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_PRESENCE_ADAPTER_H_
