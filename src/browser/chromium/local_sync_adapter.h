// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_LOCAL_SYNC_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_LOCAL_SYNC_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/local_sync.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace cromarc {

// LocalSync on Chrome's sync service and its prefs.
class LocalSyncAdapter : public LocalSync {
 public:
  LocalSyncAdapter(Profile* profile, syncer::SyncService* sync_service);
  LocalSyncAdapter(const LocalSyncAdapter&) = delete;
  LocalSyncAdapter& operator=(const LocalSyncAdapter&) = delete;
  ~LocalSyncAdapter() override;

  // LocalSync:
  bool IsEnabled() override;
  bool IsActive() override;
  void Enable(const base::FilePath& file) override;
  void Disable() override;
  void SetUpOnce() override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<syncer::SyncService> sync_service_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_LOCAL_SYNC_ADAPTER_H_
