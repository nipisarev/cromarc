// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/local_sync_adapter.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace cromarc {

namespace {

// Everything but what a password manager or the Mac keeps.
constexpr syncer::UserSelectableTypeSet kTypes = {
    syncer::UserSelectableType::kBookmarks,
    syncer::UserSelectableType::kPreferences,
    syncer::UserSelectableType::kAutofill,
    syncer::UserSelectableType::kThemes,
    syncer::UserSelectableType::kHistory,
    syncer::UserSelectableType::kExtensions,
    syncer::UserSelectableType::kApps,
    syncer::UserSelectableType::kReadingList,
    syncer::UserSelectableType::kTabs,
    syncer::UserSelectableType::kSavedTabGroups,
};

}  // namespace

LocalSyncAdapter::LocalSyncAdapter(Profile* profile,
                                   syncer::SyncService* sync_service)
    : profile_(profile), sync_service_(sync_service) {}

LocalSyncAdapter::~LocalSyncAdapter() = default;

bool LocalSyncAdapter::IsEnabled() {
  return profile_->GetPrefs()->GetBoolean(
      syncer::prefs::kEnableLocalSyncBackend);
}

bool LocalSyncAdapter::IsActive() {
  return sync_service_ && sync_service_->IsLocalSyncEnabled();
}

void LocalSyncAdapter::Enable(const base::FilePath& file) {
  PrefService* prefs = profile_->GetPrefs();
  // Despite the name, on the Mac the pref is the backend's file.
  prefs->SetFilePath(syncer::prefs::kLocalSyncBackendDir, file);
  prefs->SetBoolean(syncer::prefs::kEnableLocalSyncBackend, true);
}

void LocalSyncAdapter::Disable() {
  profile_->GetPrefs()->SetBoolean(syncer::prefs::kEnableLocalSyncBackend,
                                   false);
}

void LocalSyncAdapter::SetUpOnce() {
  if (!IsActive()) {
    return;
  }
  syncer::SyncUserSettings* settings = sync_service_->GetUserSettings();
  if (settings->IsInitialSyncFeatureSetupComplete()) {
    return;
  }
  settings->SetSelectedTypes(/*sync_everything=*/false, kTypes);
  settings->SetInitialSyncFeatureSetupComplete();
}

}  // namespace cromarc
