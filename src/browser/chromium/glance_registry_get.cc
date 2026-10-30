// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The composition root of Glance: the browser-process registry on the
// Chromium ports, with each space's tree and archive.

#include "base/no_destructor.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/chromium/glance_surface_adapter.h"
#include "cromarc/browser/ui/glance/glance_registry.h"
#include "cromarc/browser/ui/glance/glance_spaces.h"

namespace cromarc {

// static
GlanceRegistry* GlanceRegistry::Get() {
  static base::NoDestructor<GlanceSpaces> spaces(ChromiumTabs(),
                                                 ChromiumWindows());
  static base::NoDestructor<GlanceRegistry> instance(
      ChromiumTabs(), ChromiumWindows(), ChromiumAppLifecycle(),
      ChromiumGlanceSurface(), *spaces);
  return instance.get();
}

}  // namespace cromarc
