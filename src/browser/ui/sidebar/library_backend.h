// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_
#define CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "cromarc/browser/ports/library.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace cromarc {

class ArchiveService;
class Tabs;

// What the Library reads and changes for one window: its history (Zen's
// ZenLibraryHistorySection), the space's archive of closed tabs, and the
// pages it opens.
class LibraryBackend {
 public:
  using QueryHistoryCallback = LibraryHistory::QueryCallback;

  // `on_history_changed` runs, at most every half second, after history or
  // the archive change.
  LibraryBackend(BrowserWindowInterface* browser,
                 ArchiveService* archive,
                 Tabs& tabs,
                 std::unique_ptr<LibraryHistory> history,
                 base::RepeatingClosure on_history_changed);
  LibraryBackend(const LibraryBackend&) = delete;
  LibraryBackend& operator=(const LibraryBackend&) = delete;
  ~LibraryBackend();

  void QueryHistory(const std::string& query,
                    int days_old,
                    uint32_t limit,
                    mojom::HistorySort sort,
                    QueryHistoryCallback callback);
  void ForgetPage(const GURL& url);
  // The URL's site: its pages, cookies, storage and cache.
  void ForgetSite(const GURL& url);
  // Everything since `begin`; all of it for a null time.
  void ClearHistory(base::Time begin);
  void OpenUrl(const GURL& url, bool background);
  // An archived tab back as a tab, its row gone.
  void Restore(int64_t id, const GURL& url, bool background);

 private:
  void Changed();

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<ArchiveService> archive_;
  const raw_ref<Tabs> tabs_;
  const std::unique_ptr<LibraryHistory> history_;
  base::RepeatingClosure on_history_changed_;
  base::OneShotTimer changed_timer_;
  base::CallbackListSubscription history_subscription_;
  base::CallbackListSubscription archive_subscription_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_
