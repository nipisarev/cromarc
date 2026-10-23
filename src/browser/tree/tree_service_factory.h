// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_TREE_TREE_SERVICE_FACTORY_H_
#define CROMARC_BROWSER_TREE_TREE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace cromarc {

class TreeService;

class TreeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TreeService* GetForProfile(Profile* profile);
  static TreeServiceFactory* GetInstance();

  TreeServiceFactory(const TreeServiceFactory&) = delete;
  TreeServiceFactory& operator=(const TreeServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TreeServiceFactory>;

  TreeServiceFactory();
  ~TreeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_TREE_TREE_SERVICE_FACTORY_H_
