// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_FACTORY_H_
#define CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace cromarc {

class ArchiveService;

class ArchiveServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ArchiveService* GetForProfile(Profile* profile);
  static ArchiveServiceFactory* GetInstance();

  ArchiveServiceFactory(const ArchiveServiceFactory&) = delete;
  ArchiveServiceFactory& operator=(const ArchiveServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ArchiveServiceFactory>;

  ArchiveServiceFactory();
  ~ArchiveServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ARCHIVE_ARCHIVE_SERVICE_FACTORY_H_
