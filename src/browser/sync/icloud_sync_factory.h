// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SYNC_ICLOUD_SYNC_FACTORY_H_
#define CROMARC_BROWSER_SYNC_ICLOUD_SYNC_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace cromarc {

class ICloudSync;

// Builds ICloudSync with its ports: the composition root.
class ICloudSyncFactory : public ProfileKeyedServiceFactory {
 public:
  static ICloudSyncFactory* GetInstance();
  static ICloudSync* GetForProfile(Profile* profile);

  ICloudSyncFactory(const ICloudSyncFactory&) = delete;
  ICloudSyncFactory& operator=(const ICloudSyncFactory&) = delete;

 private:
  friend base::NoDestructor<ICloudSyncFactory>;

  ICloudSyncFactory();
  ~ICloudSyncFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SYNC_ICLOUD_SYNC_FACTORY_H_
