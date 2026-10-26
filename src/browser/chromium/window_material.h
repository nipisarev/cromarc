// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_WINDOW_MATERIAL_H_
#define CROMARC_BROWSER_CHROMIUM_WINDOW_MATERIAL_H_

#include "ui/gfx/native_ui_types.h"

namespace cromarc {

// Zen's window material (zen:widget/cocoa/nsCocoaWindow-mm.patch,
// ZenWindowMaterialView): what is behind the window, blurred, under all of
// `window`'s views. The window must not be opaque, as Chrome's glass frame
// leaves it (hook H14). A second call does nothing.
void InstallWindowMaterial(gfx::NativeWindow window);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_WINDOW_MATERIAL_H_
