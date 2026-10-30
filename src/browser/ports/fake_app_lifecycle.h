// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_APP_LIFECYCLE_H_
#define CROMARC_BROWSER_PORTS_FAKE_APP_LIFECYCLE_H_

#include "base/callback_list.h"
#include "cromarc/browser/ports/app_lifecycle.h"

namespace cromarc {

class FakeAppLifecycle : public AppLifecycle {
 public:
  FakeAppLifecycle();
  ~FakeAppLifecycle() override;

  // Closing all windows starts (true) or is cancelled (false).
  void CloseAllWindows(bool closing);

  // AppLifecycle:
  bool IsRestoringSession(Profile* profile) override;
  void Restart() override;
  bool IsSessionEnding() override;
  base::CallbackListSubscription OnClosingAllWindows(
      base::RepeatingCallback<void(bool closing)> callback) override;

  bool restoring = false;
  bool session_ending = false;
  int restarts = 0;

 private:
  base::RepeatingCallbackList<void(bool)> closing_all_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_APP_LIFECYCLE_H_
