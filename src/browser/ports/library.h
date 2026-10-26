// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_LIBRARY_H_
#define CROMARC_BROWSER_PORTS_LIBRARY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"

class BrowserWindowInterface;
class GURL;

namespace cromarc {

// The history of a window's profile, as the Library shows it (Zen's
// ZenLibraryHistorySection, on Chrome's history service).
class LibraryHistory {
 public:
  using QueryCallback =
      base::OnceCallback<void(std::vector<mojom::HistoryVisitPtr>)>;

  virtual ~LibraryHistory() = default;

  // Pages matching `query` of the last `days_old` days (all, at 0), at most
  // `limit`, in `sort`'s order; none without a history.
  virtual void Query(const std::string& query,
                     int days_old,
                     uint32_t limit,
                     mojom::HistorySort sort,
                     QueryCallback callback) = 0;
  virtual void ForgetPage(const GURL& url) = 0;
  // The pages, cookies, storage and cache of `site`, a registrable domain
  // (or a host without one).
  virtual void ForgetSite(const std::string& site) = 0;
  // What was visited since `begin`, or ever when it is null.
  virtual void Clear(base::Time begin) = 0;
  // Called on every visit and deletion.
  virtual base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) = 0;
};

// The downloads of a window's profile, as chrome://downloads shows them.
class Downloads {
 public:
  virtual ~Downloads() = default;

  // Oldest first.
  virtual std::vector<mojom::DownloadPtr> List() const = 0;
  virtual void Do(uint32_t id, mojom::DownloadCommand command) = 0;
  // The finished ones out of the list.
  virtual void Clear() = 0;
};

// Makes the Library's ports for a window: the composition root's.
class LibraryPorts {
 public:
  virtual ~LibraryPorts() = default;

  virtual std::unique_ptr<LibraryHistory> HistoryOf(
      BrowserWindowInterface* window) = 0;
  // `on_changed` runs, at most four times a second, after a download came,
  // changed or went.
  virtual std::unique_ptr<Downloads> DownloadsOf(
      BrowserWindowInterface* window,
      base::RepeatingClosure on_changed) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_LIBRARY_H_
