// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_theme.h"

namespace cromarc {

FakeTheme::FakeTheme() = default;
FakeTheme::~FakeTheme() = default;

void FakeTheme::SetColor(Profile* profile, SkColor color) {
  colors[profile] = color;
}

void FakeTheme::SetScheme(Profile* profile, Scheme scheme) {
  schemes[profile] = scheme;
}

FakeStripDefaults::FakeStripDefaults() = default;
FakeStripDefaults::~FakeStripDefaults() = default;

void FakeStripDefaults::Set(Profile* profile, bool collapsed, int width) {
  set[profile] = {collapsed, width};
}

}  // namespace cromarc
