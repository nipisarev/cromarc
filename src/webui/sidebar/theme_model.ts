// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A space's theme, after Zen's gradient generator
// (zen:spaces/ZenGradientGenerator.mjs): up to three colours picked on a
// colour wheel, an opacity and a grain.

export type Rgb = [number, number, number];
export type Scheme = 'auto'|'light'|'dark';

export interface SpaceTheme {
  colors: Rgb[];
  opacity: number;
  texture: number;
  scheme: Scheme;
  // How the dots follow the first one (a HARMONIES type); "" when they move
  // freely, Zen's "floating". Zen keeps it with the colours (algorithm).
  harmony: string;
}

export interface Point {
  x: number;
  y: number;
}

// Zen's MAX_DOTS and its macOS opacity range.
export const MAX_COLORS = 3;
export const MIN_OPACITY = 0.3;
export const MAX_OPACITY = 0.8;
export const DEFAULT_THEME: SpaceTheme =
    {colors: [], opacity: 0.4, texture: 0, scheme: 'auto', harmony: ''};

// Zen's colorHarmonies: the angles of the other dots from the first one.
export const HARMONIES = [
  {type: 'complementary', angles: [180]},
  {type: 'singleAnalogous', angles: [310]},
  {type: 'splitComplementary', angles: [150, 210]},
  {type: 'analogous', angles: [50, 310]},
  {type: 'triadic', angles: [120, 240]},
];

function hueToRgb(p: number, q: number, t: number): number {
  if (t < 0) {
    t += 1;
  }
  if (t > 1) {
    t -= 1;
  }
  if (t < 1 / 6) {
    return p + (q - p) * 6 * t;
  }
  if (t < 1 / 2) {
    return q;
  }
  if (t < 2 / 3) {
    return p + (q - p) * (2 / 3 - t) * 6;
  }
  return p;
}

// h, s, l in 0–1.
export function hslToRgb(h: number, s: number, l: number): Rgb {
  if (s === 0) {
    const v = Math.round(l * 255);
    return [v, v, v];
  }
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  return [
    Math.round(hueToRgb(p, q, h + 1 / 3) * 255),
    Math.round(hueToRgb(p, q, h) * 255),
    Math.round(hueToRgb(p, q, h - 1 / 3) * 255),
  ];
}

// Hue in degrees, s and l in 0–1.
export function rgbToHsl([r, g, b]: Rgb): [number, number, number] {
  r /= 255;
  g /= 255;
  b /= 255;
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const d = max - min;
  let h = 0;
  if (d !== 0) {
    if (max === r) {
      h = ((g - b) / d) % 6;
    } else if (max === g) {
      h = (b - r) / d + 2;
    } else {
      h = (r - g) / d + 4;
    }
  }
  const l = (min + max) / 2;
  const s = d === 0 ? 0 : d / (1 - Math.abs(2 * l - 1));
  return [(h * 60 + 360) % 360, s, l];
}

// Zen's getColorFromPosition on a wheel of `size`: the angle is the hue,
// and lightness grows from the centre out, saturation staying at 90–100%.
export function colorAt(point: Point, size: number): Rgb {
  const centre = size / 2;
  const distance = Math.hypot(point.x - centre, point.y - centre);
  let angle = Math.atan2(point.y - centre, point.x - centre) * 180 / Math.PI;
  if (angle < 0) {
    angle += 360;
  }
  const normalized = 1 - Math.min(distance / centre, 1);
  const saturation = 90 + (1 - normalized) * 10;
  const lightness = Math.round((1 - normalized) * 100);
  return hslToRgb(angle / 360, saturation / 100, lightness / 100);
}

// Zen's calculateInitialPosition: where a colour sits on the wheel.
export function pointOf(color: Rgb, size: number): Point {
  const [hue, , lightness] = rgbToHsl(color);
  const centre = size / 2;
  const angle = hue * Math.PI / 180;
  return {
    x: centre + centre * lightness * Math.cos(angle),
    y: centre + centre * lightness * Math.sin(angle),
  };
}

// Zen's calculateCompliments: with `count` dots, the others follow the first
// at the harmony's angles and the same distance from the centre.
export function harmonyPoints(
    first: Point, count: number, size: number, type?: string): Point[] {
  const harmony = (type ? HARMONIES.find(h => h.type === type) : undefined) ??
      HARMONIES.find(h => h.angles.length + 1 === count);
  if (!harmony || count < 2) {
    return [first];
  }
  const centre = size / 2;
  const base = Math.atan2(first.y - centre, first.x - centre) * 180 / Math.PI;
  const distance =
      Math.min(Math.hypot(first.x - centre, first.y - centre), centre);
  return [
    first,
    ...harmony.angles.map(offset => {
      const radian = ((base + offset) % 360) * Math.PI / 180;
      return {
        x: centre + distance * Math.cos(radian),
        y: centre + distance * Math.sin(radian),
      };
    }),
  ];
}

function blend(a: Rgb, b: Rgb, percent: number): Rgb {
  const p = percent / 100;
  return [
    Math.round(a[0] * p + b[0] * (1 - p)),
    Math.round(a[1] * p + b[1] * (1 - p)),
    Math.round(a[2] * p + b[2] * (1 - p)),
  ];
}

// Zen's #getSingleRGBColor on macOS: a white overlay over the colour.
function cssOf(color: Rgb, opacity: number): string {
  const overlay = 0.6;
  const alpha =
      Math.min(1, opacity + MIN_OPACITY + overlay * (1 - (opacity + MIN_OPACITY)));
  const [r, g, b] = blend(color, [255, 255, 255], alpha * 100);
  return `rgba(${r}, ${g}, ${b}, ${opacity})`;
}

// Zen's getGradient for the window background.
export function gradientOf(theme: SpaceTheme): string {
  const colors = theme.colors.map(c => cssOf(c, theme.opacity));
  const rotation = -30;
  if (colors.length === 0) {
    return 'transparent';
  }
  if (colors.length === 1) {
    return colors[0]!;
  }
  if (colors.length === 2) {
    return [
      `linear-gradient(${rotation + 180}deg, ${colors[0]} 30%, transparent 120%)`,
      `linear-gradient(${rotation}deg, ${colors[1]} 30%, transparent 120%)`,
    ].join(', ');
  }
  return [
    `linear-gradient(-5deg, ${colors[2]} 10%, transparent 80%)`,
    `radial-gradient(circle at 95% 0%, ${colors[1]} 0%, transparent 75%)`,
    `radial-gradient(circle at 0% 0%, ${colors[0]} 10%, transparent 70%)`,
  ].join(', ');
}

export function themeFromJson(json: string): SpaceTheme {
  try {
    const value = JSON.parse(json);
    return {
      colors: Array.isArray(value.colors) ?
          value.colors.slice(0, MAX_COLORS) as Rgb[] :
          [],
      opacity: Math.min(MAX_OPACITY,
                        Math.max(MIN_OPACITY, Number(value.opacity) || 0.4)),
      texture: Math.min(1, Math.max(0, Number(value.texture) || 0)),
      scheme: ['light', 'dark'].includes(value.scheme) ? value.scheme : 'auto',
      harmony: HARMONIES.some(h => h.type === value.harmony) ? value.harmony : '',
    };
  } catch {
    return {...DEFAULT_THEME};
  }
}
