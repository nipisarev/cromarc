// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_THEME_H_
#define CROMARC_BROWSER_PORTS_THEME_H_

#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace cromarc {

// Chrome's own theme of a profile, which colours its windows' chrome.
class Theme {
 public:
  enum class Scheme { kSystem, kLight, kDark };

  virtual ~Theme() = default;

  // Chrome's colour from `color` (its tonal spot variant).
  virtual void SetColor(Profile* profile, SkColor color) = 0;
  virtual void SetScheme(Profile* profile, Scheme scheme) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_THEME_H_
