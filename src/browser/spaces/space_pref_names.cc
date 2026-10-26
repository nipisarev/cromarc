// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_pref_names.h"

#include <string>

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"

namespace cromarc {

void RegisterSpacePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSpaceIconPref, std::string());
  registry->RegisterIntegerPref(kSpaceOrderPref, -1);
  registry->RegisterTimePref(kSpaceLastActivePref, base::Time());
  registry->RegisterStringPref(kSpaceThemePref, std::string());
}

}  // namespace cromarc
