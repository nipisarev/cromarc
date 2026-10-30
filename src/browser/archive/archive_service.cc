// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

base::DictValue ToValue(const ArchiveEntry& entry) {
  return base::DictValue()
      .Set("url", entry.url.spec())
      .Set("title", entry.title)
      .Set("closed_at", base::TimeToValue(entry.closed_at))
      .Set("reason", entry.reason)
      .Set("folder_path", entry.folder_path);
}

std::optional<ArchiveEntry> FromValue(const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  const std::string* url = dict->FindString("url");
  const std::string* title = dict->FindString("title");
  const std::string* reason = dict->FindString("reason");
  const std::string* folder_path = dict->FindString("folder_path");
  std::optional<base::Time> closed_at =
      base::ValueToTime(dict->Find("closed_at"));
  if (!url || !title || !reason || !folder_path || !closed_at) {
    return std::nullopt;
  }
  ArchiveEntry entry;
  entry.url = GURL(*url);
  entry.title = base::UTF8ToUTF16(*title);
  entry.closed_at = *closed_at;
  entry.reason = *reason;
  entry.folder_path = *folder_path;
  return entry;
}

}  // namespace

ArchiveService::ArchiveService(const base::FilePath& profile_path,
                               PrefService* prefs)
    : prefs_(prefs),
      archive_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               profile_path.AppendASCII("Cromarc Archive")) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::Init));
  for (const base::Value& value : prefs_->GetList(kArchivePendingPref)) {
    if (std::optional<ArchiveEntry> entry = FromValue(value)) {
      Add(*std::move(entry));
    }
  }
  prefs_->ClearPref(kArchivePendingPref);
  archive_.AsyncCall(&Archive::Expire)
      .WithArgs(base::Time::Now(), Archive::kMaxAge, Archive::kMaxRows);
}

ArchiveService::~ArchiveService() = default;

void ArchiveService::Add(ArchiveEntry entry) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::Add))
      .WithArgs(std::move(entry));
  on_changed_.Notify();
}

void ArchiveService::AddAtSessionEnd(const ArchiveEntry& entry) {
  ScopedListPrefUpdate(prefs_, kArchivePendingPref)->Append(ToValue(entry));
}

void ArchiveService::Search(const std::u16string& query,
                            size_t limit,
                            SearchCallback callback) {
  archive_.AsyncCall(&Archive::Search)
      .WithArgs(query, limit)
      .Then(std::move(callback));
}

void ArchiveService::Remove(int64_t id) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::Remove)).WithArgs(id);
  on_changed_.Notify();
}

void ArchiveService::RemoveUrl(const GURL& url) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::RemoveUrl)).WithArgs(url);
  on_changed_.Notify();
}

base::CallbackListSubscription ArchiveService::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

}  // namespace cromarc
