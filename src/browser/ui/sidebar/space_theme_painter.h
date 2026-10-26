// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SPACE_THEME_PAINTER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SPACE_THEME_PAINTER_H_

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"

class SkPath;

namespace gfx {
class Canvas;
class Rect;
class RectF;
}  // namespace gfx

namespace cromarc {

// theme_model.ts's default.
inline constexpr double kDefaultSpaceThemeOpacity = 0.4;

// A space's theme, as its editor (theme_model.ts) saves it.
struct SpaceTheme {
  // At most three.
  std::vector<SkColor> colors;
  double opacity = kDefaultSpaceThemeOpacity;
  // The grain's strength, 0..1.
  double texture = 0;
};

// The editor's JSON; what it lacks or gets wrong takes the defaults.
SpaceTheme ParseSpaceTheme(const std::string& json);

// The space theme as the window's background: Zen's getGradient
// (zen:spaces/ZenGradientGenerator.mjs) and grain (zen-browser-ui.css,
// .zen-browser-generic-background) over `bounds`. `dark` is the browser's
// dark mode, which a space theme can set.
void PaintSpaceTheme(gfx::Canvas* canvas,
                     const gfx::Rect& bounds,
                     const SpaceTheme& theme,
                     bool dark);
// While switching spaces: `from` fading into `to`, `progress` 0..1.
void PaintSpaceThemeBlend(gfx::Canvas* canvas,
                          const gfx::Rect& bounds,
                          const SpaceTheme& from,
                          const SpaceTheme& to,
                          float progress,
                          bool dark);

// Zen's .zen-toolbar-background in compact mode (zen-compact-mode.css) inside
// `path`, for the sidebar floating over the page, `bounds` being its own and
// `window` the whole window's in its coordinates. Acrylic
// (zen.theme.acrylic-sidebar), what it floats over shows through, blurred by
// its layer and at half its brightness; otherwise an opaque base. Over it the
// toolbar gradient laid out on the whole window (background-attachment:
// fixed), the grain and a light 1px edge.
void PaintFloatingSidebar(gfx::Canvas* canvas,
                          const gfx::Rect& bounds,
                          const gfx::RectF& window,
                          const SkPath& path,
                          const SpaceTheme& theme,
                          bool dark,
                          bool acrylic);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SPACE_THEME_PAINTER_H_
