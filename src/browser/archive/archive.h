// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ARCHIVE_ARCHIVE_H_
#define CROMARC_BROWSER_ARCHIVE_ARCHIVE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace cromarc {

struct ArchiveEntry {
  int64_t id = 0;
  GURL url;
  std::u16string title;
  base::Time closed_at;
  // Why the tab was archived: "idle" (12 h without use), "clear" (Today's
  // Clear), "folder" (its folder was removed), "little" (a Little window),
  // "glance" (a glance its tab took along, or one open at a quit).
  std::string reason;
  // Titles of the folders the tab was in, " / "-separated; "" for Today.
  std::string folder_path;
  // How many times this URL was archived.
  int count = 1;
};

// Tabs Cromarc closed, one SQLite file per profile (design, component 2). Not
// synced. One row per URL. Blocking: use on a sequence that may block.
class Archive {
 public:
  static constexpr base::TimeDelta kMaxAge = base::Days(90);
  static constexpr size_t kMaxRows = 50000;

  explicit Archive(const base::FilePath& db_path);
  Archive(const Archive&) = delete;
  Archive& operator=(const Archive&) = delete;
  ~Archive();

  bool Init();
  // Inserts, or updates the row with the same URL and counts it.
  bool Add(const ArchiveEntry& entry);
  // Rows whose title or URL contains every word of `query` (any case),
  // newest first. An empty query lists the newest rows.
  std::vector<ArchiveEntry> Search(const std::u16string& query, size_t limit);
  bool Remove(int64_t id);
  bool RemoveUrl(const GURL& url);
  void Expire(base::Time now,
              base::TimeDelta max_age = kMaxAge,
              size_t max_rows = kMaxRows);

 private:
  const base::FilePath db_path_;
  sql::Database db_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ARCHIVE_ARCHIVE_H_
