// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_THEME_H_
#define CROMARC_BROWSER_PORTS_FAKE_THEME_H_

#include <map>

#include "cromarc/browser/ports/strip_defaults.h"
#include "cromarc/browser/ports/theme.h"

namespace cromarc {

class FakeTheme : public Theme {
 public:
  FakeTheme();
  ~FakeTheme() override;

  // Theme:
  void SetColor(Profile* profile, SkColor color) override;
  void SetScheme(Profile* profile, Scheme scheme) override;

  std::map<Profile*, SkColor> colors;
  std::map<Profile*, Scheme> schemes;
};

class FakeStripDefaults : public StripDefaults {
 public:
  struct Defaults {
    bool collapsed = false;
    int width = 0;
  };

  FakeStripDefaults();
  ~FakeStripDefaults() override;

  // StripDefaults:
  void Set(Profile* profile, bool collapsed, int width) override;

  std::map<Profile*, Defaults> set;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_THEME_H_
