// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_prefs_factory.h"

#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace cromarc {

// static
SpacePrefsFactory* SpacePrefsFactory::GetInstance() {
  static base::NoDestructor<SpacePrefsFactory> instance;
  return instance.get();
}

SpacePrefsFactory::SpacePrefsFactory()
    : ProfileKeyedServiceFactory("CromarcSpacePrefs",
                                 ProfileSelections::BuildForRegularProfile()) {}

SpacePrefsFactory::~SpacePrefsFactory() = default;

void SpacePrefsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterSpacePrefs(registry);
}

std::unique_ptr<KeyedService>
SpacePrefsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return nullptr;
}

}  // namespace cromarc
