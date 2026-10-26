// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_STRIP_DEFAULTS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_STRIP_DEFAULTS_ADAPTER_H_

#include "cromarc/browser/ports/strip_defaults.h"

namespace cromarc {

// StripDefaults on Chrome's vertical tab strip prefs.
class StripDefaultsAdapter : public StripDefaults {
 public:
  StripDefaultsAdapter();
  StripDefaultsAdapter(const StripDefaultsAdapter&) = delete;
  StripDefaultsAdapter& operator=(const StripDefaultsAdapter&) = delete;
  ~StripDefaultsAdapter() override;

  // StripDefaults:
  void Set(Profile* profile, bool collapsed, int width) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_STRIP_DEFAULTS_ADAPTER_H_
