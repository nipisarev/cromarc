// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SYNC_ICLOUD_SYNC_H_
#define CROMARC_BROWSER_SYNC_ICLOUD_SYNC_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace cromarc {

class LocalSync;
class Profiles;

inline constexpr char kSpaceUuidPref[] = "cromarc.space.uuid";

// Sync between Macs (design, component 10, decision D12): Chromium's local
// sync backend writes the space's data to iCloud Drive/Cromarc/<space uuid>,
// and iCloud carries it to the other Mac. One Mac at a time: the backend
// keeps a single file, and the last writer wins. Passwords stay with the
// password managers; payments and cookies stay on the Mac.
class ICloudSync : public KeyedService {
 public:
  // A space found in iCloud Drive (its space.json).
  struct RemoteSpace {
    std::string uuid;
    std::string name;
    std::string icon;
  };

  // Null for profiles without one (incognito, guest).
  // (icloud_sync_factory.cc)
  static ICloudSync* Get(Profile* profile);
  // iCloud Drive/Cromarc; --cromarc-icloud-dir in tests.
  static base::FilePath Root();
  // The spaces in iCloud Drive, read off the UI thread.
  static void FindSpaces(
      base::OnceCallback<void(std::vector<RemoteSpace>)> callback);

  ICloudSync(Profile* profile,
             Profiles& profiles,
             std::unique_ptr<LocalSync> local_sync);
  ICloudSync(const ICloudSync&) = delete;
  ICloudSync& operator=(const ICloudSync&) = delete;
  ~ICloudSync() override;

  // Whether the space syncs through iCloud from the next start on.
  bool enabled() const;
  // Whether it syncs through iCloud now; differs from enabled() until the
  // browser restarts.
  bool active() const;
  // Takes effect after a restart: the sync service reads it when the profile
  // loads. Turning it on writes the folder's space.json.
  void SetEnabled(bool enabled);
  // The space's id across Macs, made on first use. `uuid` binds a new space
  // to one found in iCloud Drive.
  std::string Uuid();
  void SetUuid(const std::string& uuid);

 private:
  std::string Name() const;

  raw_ptr<Profile> profile_;
  const raw_ref<Profiles> profiles_;
  const std::unique_ptr<LocalSync> local_sync_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SYNC_ICLOUD_SYNC_H_
