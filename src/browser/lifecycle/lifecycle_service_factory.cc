// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/lifecycle_service_factory.h"

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "cromarc/browser/chromium/adapters.h"
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
// static
LifecycleService* LifecycleService::Get(Profile* profile) {
  return LifecycleServiceFactory::GetForProfile(profile);
}

LifecycleServiceFactory::LifecycleServiceFactory()
    : ProfileKeyedServiceFactory("CromarcLifecycleService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(TreeServiceFactory::GetInstance());
  DependsOn(ArchiveServiceFactory::GetInstance());
}

LifecycleServiceFactory::~LifecycleServiceFactory() = default;

// Today tabs live 12 h, restarts included, as in Arc: the browser continues
// where it left off unless the user picks otherwise in Settings. Chromium's
// own prefs are registered before keyed services' (pref_service_builder_utils).
void LifecycleServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->SetDefaultPrefValue(
      prefs::kRestoreOnStartup,
      base::Value(SessionStartupPref::kPrefValueLast));
}

std::unique_ptr<KeyedService>
LifecycleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<LifecycleService>(
      profile, TreeServiceFactory::GetForProfile(profile),
      ArchiveServiceFactory::GetForProfile(profile), ChromiumTabs(),
      ChromiumWindows(), ChromiumAppLifecycle());
}

bool LifecycleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace cromarc
