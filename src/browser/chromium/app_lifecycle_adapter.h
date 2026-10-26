// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_APP_LIFECYCLE_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_APP_LIFECYCLE_ADAPTER_H_

#include "cromarc/browser/ports/app_lifecycle.h"

namespace cromarc {

// AppLifecycle on Chrome's SessionRestore and PerformanceManager.
class AppLifecycleAdapter : public AppLifecycle {
 public:
  AppLifecycleAdapter();
  AppLifecycleAdapter(const AppLifecycleAdapter&) = delete;
  AppLifecycleAdapter& operator=(const AppLifecycleAdapter&) = delete;
  ~AppLifecycleAdapter() override;

  // AppLifecycle:
  bool IsRestoringSession(Profile* profile) override;
  bool MayHaveUserEdits(content::WebContents* contents) override;
  void Restart() override;
  bool IsSessionEnding() override;
  base::CallbackListSubscription OnClosingAllWindows(
      base::RepeatingCallback<void(bool closing)> callback) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_APP_LIFECYCLE_ADAPTER_H_
