// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_STRIP_DEFAULTS_H_
#define CROMARC_BROWSER_PORTS_STRIP_DEFAULTS_H_

class Profile;

namespace cromarc {

// What Chrome gives the vertical tab strip of a profile's new window that
// session restore does not bring back.
class StripDefaults {
 public:
  virtual ~StripDefaults() = default;

  virtual void Set(Profile* profile, bool collapsed, int width) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_STRIP_DEFAULTS_H_
