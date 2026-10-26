// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/app_lifecycle_adapter.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_restore.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"

namespace cromarc {

AppLifecycleAdapter::AppLifecycleAdapter() = default;
AppLifecycleAdapter::~AppLifecycleAdapter() = default;

bool AppLifecycleAdapter::IsRestoringSession(Profile* profile) {
  return SessionRestore::IsRestoring(profile);
}

bool AppLifecycleAdapter::MayHaveUserEdits(content::WebContents* contents) {
  if (!performance_manager::PerformanceManager::IsAvailable()) {
    return true;
  }
  base::WeakPtr<performance_manager::PageNode> page =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          contents);
  return !page || page->HadUserEdits();
}

void AppLifecycleAdapter::Restart() {
  chrome::AttemptRestart();
}

bool AppLifecycleAdapter::IsSessionEnding() {
  return browser_shutdown::GetShutdownType() ==
         browser_shutdown::ShutdownType::kEndSession;
}

base::CallbackListSubscription AppLifecycleAdapter::OnClosingAllWindows(
    base::RepeatingCallback<void(bool closing)> callback) {
  return chrome::AddClosingAllBrowsersCallback(std::move(callback));
}

}  // namespace cromarc
