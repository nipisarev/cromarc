// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_
#define CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace cromarc {

class ArchiveService;

// What the Library reads and changes for one window: its history (Zen's
// ZenLibraryHistorySection, on Chrome's history service), the space's
// archive of closed tabs, and the pages it opens.
class LibraryBackend : public history::HistoryServiceObserver {
 public:
  using QueryHistoryCallback =
      base::OnceCallback<void(std::vector<mojom::HistoryVisitPtr>)>;

  // `on_history_changed` runs, at most every half second, after history or
  // the archive change.
  LibraryBackend(BrowserWindowInterface* browser,
                 ArchiveService* archive,
                 base::RepeatingClosure on_history_changed);
  LibraryBackend(const LibraryBackend&) = delete;
  LibraryBackend& operator=(const LibraryBackend&) = delete;
  ~LibraryBackend() override;

  void QueryHistory(const std::string& query,
                    int days_old,
                    uint32_t limit,
                    mojom::HistorySort sort,
                    QueryHistoryCallback callback);
  void ForgetPage(const GURL& url);
  void ForgetSite(const GURL& url);
  // Everything since `begin`; all of it for a null time.
  void ClearHistory(base::Time begin);
  void OpenUrl(const GURL& url, bool background);
  // An archived tab back as a tab, its row gone.
  void Restore(int64_t id, const GURL& url, bool background);

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::VisitedURLInfo& visited_url_info) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  history::HistoryService* History() const;
  void Changed();

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<ArchiveService> archive_;
  base::RepeatingClosure on_history_changed_;
  base::OneShotTimer changed_timer_;
  base::CancelableTaskTracker tracker_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
  base::CallbackListSubscription archive_subscription_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_BACKEND_H_
