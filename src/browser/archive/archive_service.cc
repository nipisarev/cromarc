// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cromarc/browser/archive/archive_service_factory.h"

namespace cromarc {

// static
ArchiveService* ArchiveService::Get(Profile* profile) {
  return ArchiveServiceFactory::GetForProfile(profile);
}

ArchiveService::ArchiveService(const base::FilePath& profile_path)
    : archive_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               profile_path.AppendASCII("Cromarc Archive")) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::Init));
  archive_.AsyncCall(&Archive::Expire)
      .WithArgs(base::Time::Now(), Archive::kMaxAge, Archive::kMaxRows);
}

ArchiveService::~ArchiveService() = default;

void ArchiveService::Add(ArchiveEntry entry) {
  archive_.AsyncCall(base::IgnoreResult(&Archive::Add))
      .WithArgs(std::move(entry));
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
}

}  // namespace cromarc
