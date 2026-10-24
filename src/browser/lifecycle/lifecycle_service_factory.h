// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_FACTORY_H_
#define CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace cromarc {

class LifecycleService;

class LifecycleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static LifecycleService* GetForProfile(Profile* profile);
  static LifecycleServiceFactory* GetInstance();

  LifecycleServiceFactory(const LifecycleServiceFactory&) = delete;
  LifecycleServiceFactory& operator=(const LifecycleServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LifecycleServiceFactory>;

  LifecycleServiceFactory();
  ~LifecycleServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  // The engine runs from profile start, sidebar open or not.
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_LIFECYCLE_LIFECYCLE_SERVICE_FACTORY_H_
