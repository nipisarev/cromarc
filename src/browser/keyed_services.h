// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_KEYED_SERVICES_H_
#define CROMARC_BROWSER_KEYED_SERVICES_H_

namespace cromarc {

// Hook H8, called from EnsureBrowserContextKeyedServiceFactoriesBuilt():
// Chromium requires every keyed service factory to exist before the first
// profile is created.
void EnsureKeyedServiceFactoriesBuilt();

}  // namespace cromarc

#endif  // CROMARC_BROWSER_KEYED_SERVICES_H_
