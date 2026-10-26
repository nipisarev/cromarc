// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/history_adapter.h"

#include <algorithm>
#include <functional>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/history/history_service_factory.h"  // nogncheck: as above
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "url/gurl.h"

namespace cromarc {

HistoryAdapter::HistoryAdapter(Profile* profile) : profile_(profile) {
  if (history::HistoryService* history = History()) {
    history_observation_.Observe(history);
  }
}

HistoryAdapter::~HistoryAdapter() = default;

history::HistoryService* HistoryAdapter::History() const {
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

void HistoryAdapter::Query(const std::string& query,
                           int days_old,
                           uint32_t limit,
                           mojom::HistorySort sort,
                           QueryCallback callback) {
  history::HistoryService* history = History();
  if (!history) {
    std::move(callback).Run({});
    return;
  }
  history::QueryOptions options;
  if (days_old > 0) {
    options.begin_time = base::Time::Now() - base::Days(days_old);
  }
  // Most visited reads every page of the time and keeps the top ones.
  const bool most_visited = sort == mojom::HistorySort::kMostVisited;
  options.max_count = most_visited ? 0 : static_cast<int>(limit);
  // Firefox's history lists a page once a day by date, otherwise once.
  options.duplicate_policy =
      sort == mojom::HistorySort::kDate && query.empty()
          ? history::QueryOptions::REMOVE_DUPLICATES_PER_DAY
          : history::QueryOptions::REMOVE_ALL_DUPLICATES;
  history->QueryHistory(
      base::UTF8ToUTF16(query), options,
      base::BindOnce(
          [](bool most_visited, uint32_t limit, QueryCallback callback,
             history::QueryResults results) {
            std::vector<mojom::HistoryVisitPtr> visits;
            for (const history::URLResult& result : results) {
              visits.push_back(mojom::HistoryVisit::New(
                  result.url(), base::UTF16ToUTF8(result.title()),
                  result.visit_time().InMillisecondsFSinceUnixEpoch(),
                  result.visit_count()));
            }
            if (most_visited) {
              std::ranges::stable_sort(
                  visits, std::greater<>(),
                  [](const mojom::HistoryVisitPtr& visit) {
                    return visit->visit_count;
                  });
              if (visits.size() > limit) {
                visits.resize(limit);
              }
            }
            std::move(callback).Run(std::move(visits));
          },
          most_visited, limit, std::move(callback)),
      &tracker_);
}

void HistoryAdapter::ForgetPage(const GURL& url) {
  if (history::HistoryService* history = History()) {
    history->DeleteURLs({url});
  }
}

void HistoryAdapter::ForgetSite(const std::string& site) {
  if (history::HistoryService* history = History()) {
    history::QueryOptions options;
    options.hostname_suffix = site;
    history->QueryHistory(
        std::u16string(), options,
        base::BindOnce(
            [](base::WeakPtr<history::HistoryService> history,
               history::QueryResults results) {
              std::vector<GURL> urls;
              for (const history::URLResult& result : results) {
                urls.push_back(result.url());
              }
              if (history && !urls.empty()) {
                history->DeleteURLs(urls);
              }
            },
            history->AsWeakPtr()),
        &tracker_);
  }
  // Cookies, storage and cache. History stays out of this mask: the remover
  // clears all of it whatever the filter.
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddRegisterableDomain(site);
  profile_->GetBrowsingDataRemover()->RemoveWithFilter(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
          content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      std::move(filter));
}

void HistoryAdapter::Clear(base::Time begin) {
  if (history::HistoryService* history = History()) {
    history->ExpireHistoryBetween(std::set<GURL>(), history::kNoAppIdFilter,
                                  begin, base::Time(),
                                  /*user_initiated=*/true, base::DoNothing(),
                                  &tracker_);
  }
}

base::CallbackListSubscription HistoryAdapter::OnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void HistoryAdapter::OnURLVisited(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  on_changed_.Notify();
}

void HistoryAdapter::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  on_changed_.Notify();
}

void HistoryAdapter::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observation_.Reset();
}

}  // namespace cromarc
