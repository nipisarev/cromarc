// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_HISTORY_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_HISTORY_ADAPTER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "cromarc/browser/ports/library.h"

class Profile;

namespace cromarc {

// LibraryHistory on Chrome's history service and browsing data remover.
class HistoryAdapter : public LibraryHistory,
                       public history::HistoryServiceObserver {
 public:
  explicit HistoryAdapter(Profile* profile);
  HistoryAdapter(const HistoryAdapter&) = delete;
  HistoryAdapter& operator=(const HistoryAdapter&) = delete;
  ~HistoryAdapter() override;

  // LibraryHistory:
  void Query(const std::string& query,
             int days_old,
             uint32_t limit,
             mojom::HistorySort sort,
             QueryCallback callback) override;
  void ForgetPage(const GURL& url) override;
  void ForgetSite(const std::string& site) override;
  void Clear(base::Time begin) override;
  base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::VisitedURLInfo& visited_url_info) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  history::HistoryService* History() const;

  const raw_ptr<Profile> profile_;
  base::RepeatingClosureList on_changed_;
  base::CancelableTaskTracker tracker_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_HISTORY_ADAPTER_H_
