// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "cromarc/browser/tree/tree_service.h"

namespace cromarc {

// static
TreeService* TreeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TreeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TreeServiceFactory* TreeServiceFactory::GetInstance() {
  static base::NoDestructor<TreeServiceFactory> instance;
  return instance.get();
}

TreeServiceFactory::TreeServiceFactory()
    : ProfileKeyedServiceFactory("CromarcTreeService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

TreeServiceFactory::~TreeServiceFactory() = default;

std::unique_ptr<KeyedService>
TreeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<TreeService>(
      BookmarkModelFactory::GetForBrowserContext(context));
}

}  // namespace cromarc
