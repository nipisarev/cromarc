// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_THEME_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_THEME_ADAPTER_H_

#include "cromarc/browser/ports/theme.h"

namespace cromarc {

// Theme on Chrome's ThemeService.
class ThemeAdapter : public Theme {
 public:
  ThemeAdapter();
  ThemeAdapter(const ThemeAdapter&) = delete;
  ThemeAdapter& operator=(const ThemeAdapter&) = delete;
  ~ThemeAdapter() override;

  // Theme:
  void SetColor(Profile* profile, SkColor color) override;
  void SetScheme(Profile* profile, Scheme scheme) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_THEME_ADAPTER_H_
