// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive.h"

#include <string_view>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace cromarc {

namespace {

constexpr char kCreateTable[] =
    "CREATE TABLE IF NOT EXISTS archive("
    "id INTEGER PRIMARY KEY,"
    "url TEXT NOT NULL UNIQUE,"
    "title TEXT NOT NULL,"
    "closed_at INTEGER NOT NULL,"
    "reason TEXT NOT NULL,"
    "folder_path TEXT NOT NULL,"
    "count INTEGER NOT NULL DEFAULT 1)";
constexpr char kCreateIndex[] =
    "CREATE INDEX IF NOT EXISTS archive_closed_at ON archive(closed_at)";

// A LIKE pattern matching `word` anywhere, with LIKE's wildcards escaped.
std::u16string ContainsPattern(std::u16string_view word) {
  std::u16string pattern = u"%";
  for (char16_t c : word) {
    if (c == u'%' || c == u'_' || c == u'\\') {
      pattern += u'\\';
    }
    pattern += c;
  }
  pattern += u'%';
  return pattern;
}

ArchiveEntry ReadEntry(sql::Statement& s) {
  ArchiveEntry entry;
  entry.id = s.ColumnInt64(0);
  entry.url = GURL(s.ColumnString(1));
  entry.title = s.ColumnString16(2);
  entry.closed_at = s.ColumnTime(3);
  entry.reason = s.ColumnString(4);
  entry.folder_path = s.ColumnString(5);
  entry.count = s.ColumnInt(6);
  return entry;
}

}  // namespace

Archive::Archive(const base::FilePath& db_path)
    : db_path_(db_path), db_(sql::DatabaseOptions(), "CromarcArchive") {}

Archive::~Archive() = default;

bool Archive::Init() {
  return db_.Open(db_path_) && db_.Execute(kCreateTable) &&
         db_.Execute(kCreateIndex);
}

bool Archive::Add(const ArchiveEntry& entry) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }
  sql::Statement update(db_.GetUniqueStatement(
      "UPDATE archive SET title=?, closed_at=?, reason=?, folder_path=?, "
      "count=count+1 WHERE url=?"));
  update.BindString16(0, entry.title);
  update.BindTime(1, entry.closed_at);
  update.BindString(2, entry.reason);
  update.BindString(3, entry.folder_path);
  update.BindString(4, entry.url.spec());
  if (!update.Run()) {
    return false;
  }
  if (db_.GetLastChangeCount() == 0) {
    sql::Statement insert(db_.GetUniqueStatement(
        "INSERT INTO archive(url, title, closed_at, reason, folder_path) "
        "VALUES(?,?,?,?,?)"));
    insert.BindString(0, entry.url.spec());
    insert.BindString16(1, entry.title);
    insert.BindTime(2, entry.closed_at);
    insert.BindString(3, entry.reason);
    insert.BindString(4, entry.folder_path);
    if (!insert.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

std::vector<ArchiveEntry> Archive::Search(const std::u16string& query,
                                          size_t limit) {
  const std::vector<std::u16string_view> words = base::SplitStringPiece(
      query, base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::string sql =
      "SELECT id, url, title, closed_at, reason, folder_path, count "
      "FROM archive WHERE 1";
  for (size_t i = 0; i < words.size(); ++i) {
    sql += " AND (title LIKE ? ESCAPE '\\' OR url LIKE ? ESCAPE '\\')";
  }
  sql += " ORDER BY closed_at DESC LIMIT ?";

  sql::Statement s(db_.GetUniqueStatement(sql));
  int param = 0;
  for (std::u16string_view word : words) {
    const std::u16string pattern = ContainsPattern(word);
    s.BindString16(param++, pattern);
    s.BindString16(param++, pattern);
  }
  s.BindInt64(param, static_cast<int64_t>(limit));

  std::vector<ArchiveEntry> entries;
  while (s.Step()) {
    entries.push_back(ReadEntry(s));
  }
  return entries;
}

bool Archive::Remove(int64_t id) {
  sql::Statement s(db_.GetUniqueStatement("DELETE FROM archive WHERE id=?"));
  s.BindInt64(0, id);
  return s.Run() && db_.GetLastChangeCount() == 1;
}

bool Archive::RemoveUrl(const GURL& url) {
  sql::Statement s(db_.GetUniqueStatement("DELETE FROM archive WHERE url=?"));
  s.BindString(0, url.spec());
  return s.Run() && db_.GetLastChangeCount() == 1;
}

void Archive::Expire(base::Time now,
                     base::TimeDelta max_age,
                     size_t max_rows) {
  sql::Statement old(
      db_.GetUniqueStatement("DELETE FROM archive WHERE closed_at < ?"));
  old.BindTime(0, now - max_age);
  old.Run();

  sql::Statement over(db_.GetUniqueStatement(
      "DELETE FROM archive WHERE id NOT IN "
      "(SELECT id FROM archive ORDER BY closed_at DESC LIMIT ?)"));
  over.BindInt64(0, static_cast<int64_t>(max_rows));
  over.Run();
}

}  // namespace cromarc
