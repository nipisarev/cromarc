// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "cromarc/browser/archive/archive_service.h"

namespace cromarc {

// static
ArchiveService* ArchiveService::Get(Profile* profile) {
  return ArchiveServiceFactory::GetForProfile(profile);
}

// static
ArchiveService* ArchiveServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ArchiveService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ArchiveServiceFactory* ArchiveServiceFactory::GetInstance() {
  static base::NoDestructor<ArchiveServiceFactory> instance;
  return instance.get();
}

// Regular profiles only: off-the-record profiles never write the archive.
ArchiveServiceFactory::ArchiveServiceFactory()
    : ProfileKeyedServiceFactory("CromarcArchiveService",
                                 ProfileSelections::BuildForRegularProfile()) {}

ArchiveServiceFactory::~ArchiveServiceFactory() = default;

void ArchiveServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kArchivePendingPref);
}

std::unique_ptr<KeyedService>
ArchiveServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ArchiveService>(
      context->GetPath(), Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace cromarc
