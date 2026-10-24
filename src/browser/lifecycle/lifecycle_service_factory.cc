// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/lifecycle_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/archive/archive_service_factory.h"
#include "cromarc/browser/lifecycle/lifecycle_service.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/tree/tree_service_factory.h"

namespace cromarc {

// static
LifecycleService* LifecycleServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LifecycleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
LifecycleServiceFactory* LifecycleServiceFactory::GetInstance() {
  static base::NoDestructor<LifecycleServiceFactory> instance;
  return instance.get();
}

// Regular profiles only: in off-the-record profiles nothing is archived.
LifecycleServiceFactory::LifecycleServiceFactory()
    : ProfileKeyedServiceFactory("CromarcLifecycleService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(TreeServiceFactory::GetInstance());
  DependsOn(ArchiveServiceFactory::GetInstance());
}

LifecycleServiceFactory::~LifecycleServiceFactory() = default;

std::unique_ptr<KeyedService>
LifecycleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<LifecycleService>(
      profile, TreeServiceFactory::GetForProfile(profile),
      ArchiveServiceFactory::GetForProfile(profile));
}

bool LifecycleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace cromarc
