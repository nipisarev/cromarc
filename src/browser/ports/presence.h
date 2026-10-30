// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_PRESENCE_H_
#define CROMARC_BROWSER_PORTS_PRESENCE_H_

#include "base/time/time.h"

namespace cromarc {

// Whether the person is at the Mac.
class Presence {
 public:
  virtual ~Presence() = default;

  // How long there has been no keyboard or mouse input.
  virtual base::TimeDelta IdleFor() = 0;
  virtual bool IsScreenLocked() = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_PRESENCE_H_
