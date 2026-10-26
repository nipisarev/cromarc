// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/sync/icloud_sync_factory.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/chromium/local_sync_adapter.h"
#include "cromarc/browser/sync/icloud_sync.h"

namespace cromarc {

// static
ICloudSync* ICloudSync::Get(Profile* profile) {
  return ICloudSyncFactory::GetForProfile(profile);
}

// static
ICloudSyncFactory* ICloudSyncFactory::GetInstance() {
  static base::NoDestructor<ICloudSyncFactory> instance;
  return instance.get();
}

// static
ICloudSync* ICloudSyncFactory::GetForProfile(Profile* profile) {
  return static_cast<ICloudSync*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ICloudSyncFactory::ICloudSyncFactory()
    : ProfileKeyedServiceFactory("CromarcICloudSync",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

ICloudSyncFactory::~ICloudSyncFactory() = default;

void ICloudSyncFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kSpaceUuidPref, std::string());
}

std::unique_ptr<KeyedService>
ICloudSyncFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ICloudSync>(
      profile, ChromiumProfiles(),
      std::make_unique<LocalSyncAdapter>(
          profile, SyncServiceFactory::GetForProfile(profile)));
}

bool ICloudSyncFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace cromarc
