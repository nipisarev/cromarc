// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/strip_defaults_adapter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace cromarc {

StripDefaultsAdapter::StripDefaultsAdapter() = default;
StripDefaultsAdapter::~StripDefaultsAdapter() = default;

void StripDefaultsAdapter::Set(Profile* profile, bool collapsed, int width) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kVerticalTabsCollapsedState, collapsed);
  prefs->SetInteger(prefs::kVerticalTabsUncollapsedWidth, width);
}

}  // namespace cromarc
