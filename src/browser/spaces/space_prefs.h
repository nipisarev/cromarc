// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SPACE_PREFS_H_
#define CROMARC_BROWSER_SPACES_SPACE_PREFS_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "cromarc/browser/spaces/space_pref_names.h"

namespace cromarc {

// Registers the space prefs (space_pref_names.h) for every profile. Builds
// no service.
class SpacePrefsFactory : public ProfileKeyedServiceFactory {
 public:
  static SpacePrefsFactory* GetInstance();

  SpacePrefsFactory(const SpacePrefsFactory&) = delete;
  SpacePrefsFactory& operator=(const SpacePrefsFactory&) = delete;

 private:
  friend base::NoDestructor<SpacePrefsFactory>;

  SpacePrefsFactory();
  ~SpacePrefsFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SPACE_PREFS_H_
