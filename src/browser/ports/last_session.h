// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_LAST_SESSION_H_
#define CROMARC_BROWSER_PORTS_LAST_SESSION_H_

#include <cstddef>

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

// A profile's last session, as Chromium's session restore keeps it. Windows
// and profiles are pointers used as handles only.
class LastSession {
 public:
  virtual ~LastSession() = default;

  // Whether a first window of `profile` opened now gets its last session
  // back, by Chromium's own rule: none of its windows is open or being
  // restored, and its "On startup" continues where it left off. Asked before
  // the window opens.
  virtual bool IsDue(Profile* profile) = 0;
  // Adds the tabs of every normal window of the last session of `window`'s
  // profile at the end of `window`, in their order, without showing it.
  // Pinned tabs stay out: the sidebar brings them back asleep. The tab
  // selected in the window that was active is selected. `done` gets how many
  // tabs came, none when `window` closed meanwhile.
  virtual void RestoreInto(BrowserWindowInterface* window,
                           base::OnceCallback<void(size_t added)> done) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_LAST_SESSION_H_
