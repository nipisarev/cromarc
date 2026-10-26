// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_APP_LIFECYCLE_H_
#define CROMARC_BROWSER_PORTS_APP_LIFECYCLE_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

// The browser's own life and what it knows of its pages. Profiles are
// pointers used as handles only.
class AppLifecycle {
 public:
  virtual ~AppLifecycle() = default;

  // Session restore is still bringing back `profile`'s windows and tabs.
  virtual bool IsRestoringSession(Profile* profile) = 0;
  // The user typed or edited something on the page: true when that cannot be
  // known.
  virtual bool MayHaveUserEdits(content::WebContents* contents) = 0;
  // The process is about to exit with its tabs open: SIGTERM or logout.
  virtual bool IsSessionEnding() = 0;
  // Called when closing all windows starts (true) or is cancelled (false).
  virtual base::CallbackListSubscription OnClosingAllWindows(
      base::RepeatingCallback<void(bool closing)> callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_APP_LIFECYCLE_H_
