// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_H_
#define CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "cromarc/browser/archive/archive.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace cromarc {

// The archive of one regular profile, on a background sequence. Results come
// back on the calling sequence.
class ArchiveService : public KeyedService {
 public:
  using SearchCallback = base::OnceCallback<void(std::vector<ArchiveEntry>)>;

  static ArchiveService* Get(Profile* profile);

  explicit ArchiveService(const base::FilePath& profile_path);
  ArchiveService(const ArchiveService&) = delete;
  ArchiveService& operator=(const ArchiveService&) = delete;
  ~ArchiveService() override;

  void Add(ArchiveEntry entry);
  void Search(const std::u16string& query,
              size_t limit,
              SearchCallback callback);
  void Remove(int64_t id);

 private:
  base::SequenceBound<Archive> archive_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_H_
