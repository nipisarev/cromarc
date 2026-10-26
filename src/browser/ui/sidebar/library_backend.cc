// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/library_backend.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/tabs.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/window_open_disposition.h"

namespace cromarc {

namespace {

constexpr base::TimeDelta kChangedDelay = base::Milliseconds(500);

}  // namespace

LibraryBackend::LibraryBackend(BrowserWindowInterface* browser,
                               ArchiveService* archive,
                               Tabs& tabs,
                               std::unique_ptr<LibraryHistory> history,
                               base::RepeatingClosure on_history_changed)
    : browser_(browser),
      archive_(archive),
      tabs_(tabs),
      history_(std::move(history)),
      on_history_changed_(std::move(on_history_changed)) {
  history_subscription_ = history_->OnChanged(
      base::BindRepeating(&LibraryBackend::Changed, base::Unretained(this)));
  archive_subscription_ = archive_->RegisterOnChanged(base::BindRepeating(
      &LibraryBackend::Changed, base::Unretained(this)));
}

LibraryBackend::~LibraryBackend() = default;

void LibraryBackend::QueryHistory(const std::string& query,
                                  int days_old,
                                  uint32_t limit,
                                  mojom::HistorySort sort,
                                  QueryHistoryCallback callback) {
  history_->Query(query, days_old, limit, sort, std::move(callback));
}

void LibraryBackend::ForgetPage(const GURL& url) {
  history_->ForgetPage(url);
}

void LibraryBackend::ForgetSite(const GURL& url) {
  std::string site = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (site.empty()) {
    // A host without a registrable domain, such as an IP address, stands for
    // itself.
    site = url.GetHost();
  }
  if (site.empty()) {
    return;
  }
  history_->ForgetSite(site);
}

void LibraryBackend::ClearHistory(base::Time begin) {
  history_->Clear(begin);
}

void LibraryBackend::OpenUrl(const GURL& url, bool background) {
  tabs_->Open(browser_, url,
              background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                         : WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void LibraryBackend::Restore(int64_t id, const GURL& url, bool background) {
  OpenUrl(url, background);
  archive_->Remove(id);
}

void LibraryBackend::Changed() {
  if (!changed_timer_.IsRunning()) {
    changed_timer_.Start(FROM_HERE, kChangedDelay, on_history_changed_);
  }
}

}  // namespace cromarc
