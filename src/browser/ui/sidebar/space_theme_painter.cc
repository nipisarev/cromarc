// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/space_theme_painter.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_shader.h"
#include "chrome/grit/cromarc_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradient.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/scoped_canvas.h"

namespace cromarc {

namespace {

// zen:spaces/ZenGradientGenerator.mjs (MIN_OPACITY, MAX_OPACITY, MAX_DOTS)
// and the defaults of theme_model.ts.
constexpr double kMinOpacity = 0.3;
constexpr double kMaxOpacity = 0.8;
constexpr size_t kMaxColors = 3;
// getGradient's rotation.
constexpr float kRotation = -30;

}  // namespace

SpaceTheme ParseSpaceTheme(const std::string& json) {
  SpaceTheme theme;
  std::optional<base::Value> value = base::JSONReader::Read(json, 0);
  const base::DictValue* dict = value ? value->GetIfDict() : nullptr;
  if (!dict) {
    return theme;
  }
  if (const base::ListValue* colors = dict->FindList("colors")) {
    for (const base::Value& color : *colors) {
      const base::ListValue* rgb = color.GetIfList();
      if (!rgb || rgb->size() != 3 || theme.colors.size() == kMaxColors) {
        continue;
      }
      theme.colors.push_back(SkColorSetRGB((*rgb)[0].GetIfInt().value_or(0),
                                           (*rgb)[1].GetIfInt().value_or(0),
                                           (*rgb)[2].GetIfInt().value_or(0)));
    }
  }
  theme.opacity = std::clamp(dict->FindDouble("opacity").value_or(kDefaultSpaceThemeOpacity),
                             kMinOpacity, kMaxOpacity);
  theme.texture = std::clamp(dict->FindDouble("texture").value_or(0.0), 0.0, 1.0);
  return theme;
}

namespace {

// blendColors: `a` at `percent` over `b`.
SkColor Blend(SkColor a, SkColor b, double percent) {
  const double p = percent / 100;
  const auto mix = [p](U8CPU x, U8CPU y) {
    return static_cast<U8CPU>(std::round(x * p + y * (1 - p)));
  };
  return SkColorSetRGB(mix(SkColorGetR(a), SkColorGetR(b)),
                       mix(SkColorGetG(a), SkColorGetG(b)),
                       mix(SkColorGetB(a), SkColorGetB(b)));
}

// #getSingleRGBColor with blendWithWhiteOverlay on macOS, where the window
// can be transparent.
SkColor4f ColorOf(SkColor color, double opacity) {
  constexpr double kOverlay = 0.6;
  const double alpha =
      std::min(1.0, opacity + kMinOpacity +
                        kOverlay * (1 - (opacity + kMinOpacity)));
  SkColor4f result =
      SkColor4f::FromColor(Blend(color, SK_ColorWHITE, alpha * 100));
  result.fA = opacity;
  return result;
}

// A CSS colour stop; positions may lie outside 0..1.
struct Stop {
  float pos;
  SkColor4f color;
};

SkColor4f Transparent(SkColor4f color) {
  color.fA = 0;
  return color;
}

SkColor4f Mix(const Stop& a, const Stop& b, float pos) {
  const float t = (pos - a.pos) / (b.pos - a.pos);
  return {a.color.fR + (b.color.fR - a.color.fR) * t,
          a.color.fG + (b.color.fG - a.color.fG) * t,
          a.color.fB + (b.color.fB - a.color.fB) * t,
          a.color.fA + (b.color.fA - a.color.fA) * t};
}

// Two CSS stops as Skia's, which must lie within 0..1: the colour before the
// first stop and after the last one is theirs, as in CSS.
void ToSkia(const Stop& a,
            const Stop& b,
            std::vector<SkColor4f>* colors,
            std::vector<float>* positions) {
  const auto add = [&](float pos, SkColor4f color) {
    colors->push_back(color);
    positions->push_back(pos);
  };
  add(0, a.pos > 0 ? a.color : Mix(a, b, 0));
  if (a.pos > 0 && a.pos < 1) {
    add(a.pos, a.color);
  }
  if (b.pos > 0 && b.pos < 1) {
    add(b.pos, b.color);
  }
  add(1, b.pos < 1 ? b.color : Mix(a, b, 1));
}

SkGradient::Interpolation Premul() {
  // CSS gradients interpolate premultiplied, so "transparent" fades the
  // colour's alpha instead of turning it grey.
  SkGradient::Interpolation interpolation;
  interpolation.fInPremul = SkGradient::Interpolation::InPremul::kYes;
  return interpolation;
}

// linear-gradient(`degrees`, a, b) over `box`.
sk_sp<cc::PaintShader> Linear(const gfx::RectF& box,
                              float degrees,
                              const Stop& a,
                              const Stop& b) {
  const float radians = degrees * std::numbers::pi_v<float> / 180;
  const gfx::Vector2dF direction(std::sin(radians), -std::cos(radians));
  const float length = std::abs(box.width() * std::sin(radians)) +
                       std::abs(box.height() * std::cos(radians));
  const gfx::PointF center = box.CenterPoint();
  const SkPoint points[] = {
      {center.x() - direction.x() * length / 2,
       center.y() - direction.y() * length / 2},
      {center.x() + direction.x() * length / 2,
       center.y() + direction.y() * length / 2}};
  std::vector<SkColor4f> colors;
  std::vector<float> positions;
  ToSkia(a, b, &colors, &positions);
  return cc::PaintShader::MakeLinearGradient(
      points, colors.data(), positions.data(), colors.size(),
      SkTileMode::kClamp, Premul());
}

// radial-gradient(circle at `x` `y`, a, b) over `box`: farthest-corner.
sk_sp<cc::PaintShader> Radial(const gfx::RectF& box,
                              float x,
                              float y,
                              const Stop& a,
                              const Stop& b) {
  const gfx::PointF center(box.x() + box.width() * x,
                           box.y() + box.height() * y);
  float radius = 0;
  for (const gfx::PointF& corner :
       {box.origin(), box.top_right(), box.bottom_left(), box.bottom_right()}) {
    radius = std::max(radius, (corner - center).Length());
  }
  std::vector<SkColor4f> colors;
  std::vector<float> positions;
  ToSkia(a, b, &colors, &positions);
  return cc::PaintShader::MakeRadialGradient(
      SkPoint::Make(center.x(), center.y()), radius, colors.data(),
      positions.data(), colors.size(), SkTileMode::kClamp, Premul());
}

void Fill(gfx::Canvas* canvas,
          const gfx::Rect& bounds,
          sk_sp<cc::PaintShader> shader) {
  cc::PaintFlags flags;
  flags.setShader(std::move(shader));
  canvas->DrawRect(gfx::RectF(bounds), flags);
}

// Zen's grain-bg.png, from the sidebar's resources.
const cc::PaintImage& Grain() {
  static const base::NoDestructor<cc::PaintImage> grain([] {
    const SkBitmap bitmap = gfx::PNGCodec::Decode(base::as_byte_span(
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_CROMARC_SIDEBAR_IMAGES_GRAIN_BG_PNG)));
    return cc::PaintImage::CreateFromBitmap(bitmap);
  }());
  return *grain;
}

// .zen-browser-grain: the texture over `box` at the theme's strength.
void PaintGrain(gfx::Canvas* canvas, const gfx::RectF& box, double texture) {
  if (texture <= 0) {
    return;
  }
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeImage(Grain(), SkTileMode::kRepeat,
                                             SkTileMode::kRepeat, nullptr));
  flags.setAlphaf(static_cast<float>(texture));
  canvas->DrawRect(box, flags);
}

// getGradient's layers and the grain for `theme` over `bounds`.
void PaintLayers(gfx::Canvas* canvas,
                const gfx::Rect& bounds,
                const SpaceTheme& theme,
                bool dark) {
  const gfx::RectF box(bounds);
  std::vector<SkColor4f> colors;
  for (SkColor color : theme.colors) {
    colors.push_back(ColorOf(color, theme.opacity));
  }
  switch (colors.size()) {
    case 0:
      // The window's own translucency, darkened in dark mode.
      if (dark) {
        canvas->DrawColor(SkColorSetARGB(0x66, 0, 0, 0));
      }
      break;
    case 1:
      canvas->DrawColor(colors[0].toSkColor());
      break;
    case 2:
      // Listed top first; painted bottom first.
      Fill(canvas, bounds,
           Linear(box, kRotation, {0.3f, colors[1]},
                  {1.2f, Transparent(colors[1])}));
      Fill(canvas, bounds,
           Linear(box, kRotation + 180, {0.3f, colors[0]},
                  {1.2f, Transparent(colors[0])}));
      break;
    default:
      Fill(canvas, bounds,
           Radial(box, 0, 0, {0.1f, colors[0]},
                  {0.7f, Transparent(colors[0])}));
      Fill(canvas, bounds,
           Radial(box, 0.95f, 0, {0, colors[1]},
                  {0.75f, Transparent(colors[1])}));
      Fill(canvas, bounds,
           Linear(box, -5, {0.1f, colors[2]}, {0.8f, Transparent(colors[2])}));
      break;
  }
  PaintGrain(canvas, box, theme.texture);
}

// getToolbarModifiedBaseRaw with zen.theme.acrylic-sidebar on: 0.6 opaque.
SkColor ToolbarBase(bool dark) {
  return SkColorSetA(
      dark ? SkColorSetRGB(23, 23, 26) : SkColorSetRGB(240, 240, 244), 0x99);
}

// getGradient(colors, forToolbar) over `box`, and the grain:
// #zen-toolbar-background holds a .zen-browser-grain of its own. Acrylic,
// #getSingleRGBColor gives the toolbar the window's own colours.
void PaintToolbarTheme(gfx::Canvas* canvas,
                       const gfx::Rect& bounds,
                       const gfx::RectF& box,
                       const SpaceTheme& theme,
                       bool dark) {
  std::vector<SkColor4f> colors;
  for (SkColor color : theme.colors) {
    colors.push_back(ColorOf(color, theme.opacity));
  }
  switch (colors.size()) {
    case 0:
      canvas->DrawColor(ToolbarBase(dark));
      break;
    case 1:
      canvas->DrawColor(colors[0].toSkColor());
      break;
    case 2:
      Fill(canvas, bounds,
           Linear(box, kRotation, {0, colors[1]}, {1, colors[0]}));
      break;
    default:
      Fill(canvas, bounds,
           Radial(box, 0, 0, {0.1f, colors[0]},
                  {0.7f, Transparent(colors[0])}));
      Fill(canvas, bounds,
           Radial(box, 0.95f, 0, {0, colors[1]},
                  {0.75f, Transparent(colors[1])}));
      Fill(canvas, bounds,
           Linear(box, -5, {0.1f, colors[2]}, {0.8f, Transparent(colors[2])}));
      break;
  }
  PaintGrain(canvas, gfx::RectF(bounds), theme.texture);
}

}  // namespace

void PaintSpaceTheme(gfx::Canvas* canvas,
                     const gfx::Rect& bounds,
                     const SpaceTheme& theme,
                     bool dark) {
  PaintLayers(canvas, bounds, theme, dark);
}

void PaintSpaceThemeBlend(gfx::Canvas* canvas,
                          const gfx::Rect& bounds,
                          const SpaceTheme& from,
                          const SpaceTheme& to,
                          float progress,
                          bool dark) {
  // Zen's ::before (the old theme) and ::after (the new one) at
  // 1 - --zen-background-opacity and --zen-background-opacity.
  progress = std::clamp(progress, 0.0f, 1.0f);
  canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(255 * (1 - progress)));
  PaintLayers(canvas, bounds, from, dark);
  canvas->Restore();
  canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(255 * progress));
  PaintLayers(canvas, bounds, to, dark);
  canvas->Restore();
}

void PaintFloatingSidebar(gfx::Canvas* canvas,
                          const gfx::Rect& bounds,
                          const gfx::RectF& window,
                          const SkPath& path,
                          const SpaceTheme& theme,
                          bool dark,
                          bool acrylic) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->ClipPath(path, true);
  if (acrylic) {
    // backdrop-filter: brightness(0.5).
    canvas->DrawColor(SkColorSetA(SK_ColorBLACK, 0x80));
  } else {
    canvas->DrawColor(dark ? SkColorSetRGB(0x13, 0x13, 0x13)
                           : SkColorSetRGB(0xe9, 0xe9, 0xe9));
  }
  PaintToolbarTheme(canvas, bounds, window, theme, dark);
  cc::PaintFlags edge;
  edge.setStyle(cc::PaintFlags::kStroke_Style);
  edge.setStrokeWidth(2);
  edge.setAntiAlias(true);
  edge.setColor(SkColorSetA(SK_ColorWHITE, 0x26));
  canvas->DrawPath(path, edge);
}

}  // namespace cromarc
