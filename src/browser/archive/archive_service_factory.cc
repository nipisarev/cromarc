// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "cromarc/browser/archive/archive_service.h"

namespace cromarc {

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

std::unique_ptr<KeyedService>
ArchiveServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ArchiveService>(context->GetPath());
}

}  // namespace cromarc
