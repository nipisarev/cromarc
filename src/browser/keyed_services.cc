// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/keyed_services.h"

#include "cromarc/browser/tree/tree_service_factory.h"

namespace cromarc {

void EnsureKeyedServiceFactoriesBuilt() {
  TreeServiceFactory::GetInstance();
}

}  // namespace cromarc
