// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/last_session_adapter.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/session_storage_namespace_handle.h"
#include "content/public/browser/storage_partition.h"

namespace cromarc {

namespace {

// As SessionRestoreImpl adds a saved window's tabs to a browser.
void AddTabs(SessionID window_id,
             base::OnceCallback<void(size_t)> done,
             std::vector<std::unique_ptr<sessions::SessionWindow>> windows,
             SessionID active_window_id,
             bool read_error) {
  BrowserWindowInterface* browser =
      BrowserWindowInterface::FromSessionID(window_id);
  if (!browser) {
    std::move(done).Run(0);
    return;
  }
  Profile* profile = browser->GetProfile();
  TabStripModel* strip = browser->GetTabStripModel();
  // Anchored to now, as the last active time (a base::Time) maps onto the
  // TimeTicks a WebContents wants.
  const base::Time now = base::Time::Now();
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  size_t added = 0;
  std::optional<int> selected;
  for (const std::unique_ptr<sessions::SessionWindow>& window : windows) {
    if (window->type != sessions::SessionWindow::TYPE_NORMAL) {
      continue;
    }
    for (int i = 0; i < static_cast<int>(window->tabs.size()); ++i) {
      const sessions::SessionTab& tab = *window->tabs[i];
      if (tab.pinned || tab.navigations.empty()) {
        continue;
      }
      scoped_refptr<content::SessionStorageNamespaceHandle> storage;
      if (!tab.session_storage_persistent_id.empty()) {
        storage = profile->GetDefaultStoragePartition()
                      ->GetDOMStorageContext()
                      ->RecreateSessionStorage(tab.session_storage_persistent_id);
      }
      const int index = strip->count();
      // Added in the background: a selected tab would load at once and could
      // bring the window forward; the hidden window loads it when shown.
      chrome::AddRestoredTab(
          browser, tab.navigations, index, GetNavigationIndexToSelect(tab),
          tab.extension_app_id, /*group=*/std::nullopt, /*select=*/false,
          /*pin=*/false, now_ticks - (now - tab.last_active_time),
          tab.last_active_time, storage.get(), tab.user_agent_override,
          tab.extra_data, /*from_session_restore=*/true,
          /*is_active_browser=*/std::nullopt);
      if (window->window_id == active_window_id &&
          i == window->selected_tab_index) {
        selected = index;
      }
      ++added;
    }
  }
  if (selected) {
    strip->ActivateTabAt(*selected);
  }
  std::move(done).Run(added);
}

}  // namespace

LastSessionAdapter::LastSessionAdapter() = default;
LastSessionAdapter::~LastSessionAdapter() = default;

bool LastSessionAdapter::IsDue(Profile* profile) {
  SessionService* service =
      SessionServiceFactory::GetForProfileForSessionRestore(profile);
  return service && service->ShouldRestore(nullptr) &&
         SessionStartupPref::GetStartupPref(profile->GetPrefs())
             .ShouldRestoreLastSession();
}

void LastSessionAdapter::RestoreInto(
    BrowserWindowInterface* window,
    base::OnceCallback<void(size_t added)> done) {
  SessionService* service =
      SessionServiceFactory::GetForProfileForSessionRestore(
          window->GetProfile());
  if (!service) {
    std::move(done).Run(0);
    return;
  }
  service->GetLastSession(
      base::BindOnce(&AddTabs, window->GetSessionID(), std::move(done)));
}

}  // namespace cromarc
