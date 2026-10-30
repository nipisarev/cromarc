// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A space's theme, after Zen's gradient generator
// (zen:spaces/ZenGradientGenerator.mjs): up to three colours picked on the
// theme picker's pad, an opacity and a grain; and the picker's own maths.

export type Rgb = [number, number, number];
export type Scheme = 'auto'|'light'|'dark';
// How a dot reads its colour from where it sits (Zen's dot types): preset
// colours keep their lightness, dots placed by hand get it from the distance
// to the centre, the grey presets have no hue.
export type DotType = 'explicit-lightness'|'float'|'explicit-black-white';

export interface Point {
  x: number;
  y: number;
}

// Where a colour's dot sits on the pad and how it reads its colour: Zen keeps
// both with each colour (position, type).
export interface Dot extends Point {
  type: DotType;
}

export interface SpaceTheme {
  colors: Rgb[];
  // The colours' dots, in the same order; empty for themes saved before the
  // dots kept their place.
  dots: Dot[];
  // Zen's #currentLightness: what explicit-lightness dots keep.
  lightness: number;
  opacity: number;
  texture: number;
  scheme: Scheme;
  // Zen's algorithm (useAlgo): a HARMONIES type the other dots follow the
  // first one in, 'float' for a single preset colour, or "".
  harmony: string;
}

// Zen's MAX_DOTS and its macOS opacity range.
export const MAX_COLORS = 3;
export const MIN_OPACITY = 0.3;
export const MAX_OPACITY = 0.8;
export const DEFAULT_THEME: SpaceTheme = {
  colors: [],
  dots: [],
  lightness: 50,
  opacity: 0.4,
  texture: 0,
  scheme: 'auto',
  harmony: '',
};

// Zen's colorHarmonies: the angles of the other dots from the first one.
export const HARMONIES = [
  {type: 'complementary', angles: [180]},
  {type: 'singleAnalogous', angles: [310]},
  {type: 'splitComplementary', angles: [150, 210]},
  {type: 'analogous', angles: [50, 310]},
  {type: 'triadic', angles: [120, 240]},
  {type: 'floating', angles: [] as number[]},
];

// The pad: zen-gradient-generator.css, --panel-width 380px less
// --panel-padding 10px each side, and its min-height 2px shorter.
export const PAD = {width: 360, height: 358};

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

// Zen's getColorFromPosition: the colour of a dot at `point` on the pad, the
// angle round the centre being the hue. Its own offsets are kept (30 round
// the pad, 29 for half the dot). A float dot sets the lightness from its
// distance to the centre, which Zen keeps as the current lightness; returned
// as `lightness`.
export function colorFromPosition(
    point: Point, type: DotType, lightness: number,
    pad = PAD): {color: Rgb, lightness: number} {
  const padding = 30;
  const dotHalfSize = 29;
  const x = point.x + dotHalfSize;
  const y = point.y + dotHalfSize;
  const width = pad.width + padding * 2;
  const height = pad.height + padding * 2;
  const centerX = width / 2;
  const centerY = height / 2;
  const radius = (width - padding) / 2;
  const distance = Math.sqrt((x - centerX) ** 2 + (y - centerY) ** 2) - 0.2;
  let angle = Math.atan2(y - centerY, x - centerX) * 180 / Math.PI;
  if (angle < 0) {
    angle += 360;
  }
  const normalizedDistance = 1 - Math.min(distance / radius, 1);
  let saturation = normalizedDistance * 100;
  if (type !== 'explicit-lightness') {
    saturation = 90 + (1 - normalizedDistance) * 10;
    lightness = Math.round((1 - normalizedDistance) * 100);
  }
  let dotLightness = lightness;
  if (type === 'explicit-black-white') {
    saturation = 0;
    dotLightness = Math.round((1 - normalizedDistance) * 100);
  }
  const color = hslToRgb(angle / 360, saturation / 100, dotLightness / 100)
                    .map(v => Math.min(255, Math.max(0, v))) as Rgb;
  return {color, lightness};
}

// Where a colour saved without its dot goes: the inverse of
// colorFromPosition for a float dot (hue round the centre, lightness out
// from it). Zen's own calculateInitialPosition is for the same case but
// puts every colour near the centre.
export function initialPosition(color: Rgb, pad = PAD): Point {
  const [hue, , lightness] = rgbToHsl(color);
  const radius = (pad.width + 30) / 2;
  const angle = hue * Math.PI / 180;
  return clampToPad({
    x: pad.width / 2 + 1 + radius * lightness * Math.cos(angle),
    y: pad.height / 2 + 1 + radius * lightness * Math.sin(angle),
  }, pad);
}

// Zen keeps dots inside the circle the pad's width spans: one outside goes
// to its edge (onDotMouseMove, onThemePickerClick).
export function clampToPad(point: Point, pad = PAD): Point {
  const centerX = pad.width / 2;
  const centerY = pad.height / 2;
  const radius = pad.width / 2;
  const distance = Math.hypot(point.x - centerX, point.y - centerY);
  if (distance <= radius) {
    return point;
  }
  const angle = Math.atan2(point.y - centerY, point.x - centerX);
  return {
    x: centerX + Math.cos(angle) * radius,
    y: centerY + Math.sin(angle) * radius,
  };
}

export interface PickerDot {
  // 0 is the primary dot, the others follow it.
  id: number;
  position: Point;
  type: DotType;
}

// Zen's calculateCompliments: the dots' places after `action`, the others
// at the harmony's angles from the primary one and its distance from the
// centre. `count` is how many dots the picker has now. Returns the harmony
// it settled on (Zen sets useAlgo to it), or null when none applies and the
// dots stay. Zen keeps the dots passed in, unplaced, when the picker had
// none yet (a three-colour preset on an empty pad); here the primary one
// always leads.
export function calculateCompliments(
    dots: PickerDot[], count: number, action: 'add'|'remove'|'update',
    useHarmony: string,
    pad = PAD): {dots: PickerDot[], harmony: string|null} {
  if (dots.length === 0) {
    return {dots: [], harmony: null};
  }
  const numDots =
      dots.length + (action === 'add' ? 1 : action === 'remove' ? -1 : 0);
  const harmony = harmonyFor(numDots, count, action, useHarmony);
  if (!harmony) {
    return {dots, harmony: null};
  }
  if (harmony.angles.length === 0) {
    return {dots, harmony: harmony.type};
  }
  const primary = dots.find(dot => dot.id === 0);
  if (!primary) {
    return {dots: [], harmony: harmony.type};
  }
  const center = {x: pad.width / 2, y: pad.height / 2};
  const baseAngle = (Math.atan2(primary.position.y - center.y,
                                primary.position.x - center.x) *
                         180 / Math.PI +
                     360) %
      360;
  const distance = Math.min(
      Math.hypot(primary.position.x - center.x, primary.position.y - center.y),
      pad.width / 2);
  return {
    dots: [
      {id: 0, position: primary.position, type: primary.type},
      ...harmony.angles.map((offset, index) => {
        const radian = ((baseAngle + offset) % 360) * Math.PI / 180;
        return {
          id: index + 1,
          position: {
            x: center.x + distance * Math.cos(radian),
            y: center.y + distance * Math.sin(radian),
          },
          type: primary.type,
        };
      }),
    ],
    harmony: harmony.type,
  };
}

// calculateCompliments' getColorHarmonyType.
function harmonyFor(
    numDots: number, count: number, action: 'add'|'remove'|'update',
    useHarmony: string) {
  const selected = HARMONIES.find(h => h.type === useHarmony);
  if (useHarmony !== '' && selected) {
    if (action === 'remove') {
      return count !== 0 ?
          HARMONIES.find(h => h.angles.length === selected.angles.length - 1) :
          HARMONIES.find(h => h.type === 'floating');
    }
    if (action === 'add') {
      return HARMONIES.find(
          h => h.angles.length === selected.angles.length + 1);
    }
    return selected;
  }
  if (action === 'remove') {
    let harmony = HARMONIES.find(h => h.angles.length === numDots);
    // From three analogous dots, two left are single analogous.
    if (harmony?.type === 'analogous' && numDots === 1) {
      harmony = HARMONIES.find(h => h.type === 'singleAnalogous');
    }
    return harmony;
  }
  return HARMONIES.find(h => h.angles.length + 1 === numDots);
}

// The harmonies the toggle cycles through for `count` dots.
export function harmoniesFor(count: number): string[] {
  return HARMONIES.filter(h => h.angles.length + 1 === count).map(h => h.type);
}

// The opacity slider's wave (Zen's #linePath, #sinePath).
export const LINE_PATH = 'M 51.373 27.395 L 367.037 27.395';
export const SINE_PATH =
    'M 51.373 27.395 C 60.14 -8.503 68.906 -8.503 77.671 27.395 C 86.438 63.293 95.205 63.293 103.971 27.395 C 112.738 -8.503 121.504 -8.503 130.271 27.395 C 139.037 63.293 147.803 63.293 156.57 27.395 C 165.335 -8.503 174.101 -8.503 182.868 27.395 C 191.634 63.293 200.4 63.293 209.167 27.395 C 217.933 -8.503 226.7 -8.503 235.467 27.395 C 244.233 63.293 252.999 63.293 261.765 27.395 C 270.531 -8.503 279.297 -8.503 288.064 27.395 C 296.83 63.293 305.596 63.293 314.363 27.395 C 323.13 -8.503 331.896 -8.503 340.662 27.395 M 314.438 27.395 C 323.204 -8.503 331.97 -8.503 340.737 27.395 C 349.503 63.293 358.27 63.293 367.037 27.395';

type PathPoint = {type: 'M'|'L', x: number, y: number}|{
  type: 'C',
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  x: number,
  y: number,
};

// Zen's parseSinePath.
function parsePath(path: string): PathPoint[] {
  const points: PathPoint[] = [];
  for (const command of path.match(/[MCL]\s*[\d\s.\-,]+/g) ?? []) {
    const type = command.charAt(0);
    const coords = command.slice(1).trim().split(/[\s,]+/).map(Number);
    if (type === 'C') {
      for (let i = 0; i + 5 < coords.length; i += 6) {
        points.push({
          type: 'C',
          x1: coords[i]!,
          y1: coords[i + 1]!,
          x2: coords[i + 2]!,
          y2: coords[i + 3]!,
          x: coords[i + 4]!,
          y: coords[i + 5]!,
        });
      }
    } else {
      points.push({type: type as 'M' | 'L', x: coords[0]!, y: coords[1]!});
    }
  }
  return points;
}

const SINE_POINTS = parsePath(SINE_PATH);

// Zen's #interpolateWavePath: a flat line at 0, the full wave at 1.
export function wavePath(progress: number): string {
  const referenceY = 27.3;
  if (progress <= 0.001) {
    return LINE_PATH;
  }
  if (progress >= 0.999) {
    return SINE_PATH;
  }
  const at = (y: number) => referenceY + (y - referenceY) * progress;
  return SINE_POINTS
      .map(p => {
        switch (p.type) {
          case 'M':
            return `M ${p.x} ${at(p.y)}`;
          case 'C':
            return `C ${p.x1} ${at(p.y1)} ${p.x2} ${at(p.y2)} ${p.x} ${at(p.y)}`;
          case 'L':
            return `L ${p.x} ${p.y}`;
        }
      })
      .join(' ');
}

// How far the opacity is along its range, as onWorkspaceChange draws the
// wave: full for the default theme.
export function opacityProgress(opacity: number, isDefault: boolean): number {
  if (isDefault || opacity > MAX_OPACITY) {
    return 1;
  }
  if (opacity < MIN_OPACITY) {
    return 0;
  }
  return (opacity - MIN_OPACITY) / (MAX_OPACITY - MIN_OPACITY);
}

// Zen's onTextureMouseMove: the grain from where the pointer is round the
// dial's centre, 0 at the top, clockwise, in steps of 1/16.
export function textureAt(dx: number, dy: number): number {
  let texture = Math.atan2(dy, dx) * 180 / Math.PI + 90;
  if (texture < 0) {
    texture += 360;
  }
  texture = Math.round(texture / 360 * 16) / 16;
  return texture === 1 ? 0 : texture;
}

// The dial's 16 dots, the first one at 3 o'clock: lit up to the grain, as
// onWorkspaceChange counts them from the fifth (the one at the top).
export function textureDotLit(index: number, texture: number): boolean {
  return ((index + 4) % 16) / 16 <= texture;
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

const DOT_TYPES: DotType[] = ['explicit-lightness', 'float', 'explicit-black-white'];

function isRgb(value: unknown): value is Rgb {
  return Array.isArray(value) && value.length === 3 &&
      value.every(v => typeof v === 'number');
}

// ZenGradientGenerator.luminance: relative luminance (sRGB).
export function luminance([r, g, b]: Rgb): number {
  const [lr, lg, lb] = [r, g, b].map(v => {
    v /= 255;
    return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
  }) as Rgb;
  return lr * 0.2126 + lg * 0.7152 + lb * 0.0722;
}

// ZenGradientGenerator.contrastRatio.
export function contrastRatio(a: Rgb, b: Rgb): number {
  const la = luminance(a);
  const lb = luminance(b);
  return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
}

// ZenGradientGenerator.blendColors: `percentage` of `a`, the rest of `b`.
export function blendColors(a: Rgb, b: Rgb, percentage: number): Rgb {
  const p = percentage / 100;
  return [
    Math.round(a[0] * p + b[0] * (1 - p)),
    Math.round(a[1] * p + b[1] * (1 - p)),
    Math.round(a[2] * p + b[2] * (1 - p)),
  ];
}

// zen.theme.dark-mode-bias (prefs/zen/theme.yaml).
export const DARK_MODE_BIAS = 0.3;

// ZenGradientGenerator.getToolbarColor in a window that can be transparent
// (canBeTransparent, as macOS windows are): text at .9, a quarter of the
// accent mixed in when there is one.
export function toolbarColor(isDark: boolean, accent?: Rgb): [number, number, number, number] {
  const opacity = 0.9;
  const base: Rgb = isDark ? [255, 255, 255] : [0, 0, 0];
  return [...(accent ? blendColors(accent, base, 25) : base), opacity] as
      [number, number, number, number];
}

// ZenGradientGenerator.shouldBeDarkMode in a window that can be transparent:
// light text when it reads better on `accent` than dark text, the light one
// counted fainter by the bias.
export function shouldBeDarkMode(accent: Rgb): boolean {
  const dark = toolbarColor(true);
  const light = toolbarColor(false);
  light[3] -= DARK_MODE_BIAS;
  const darkText = blendColors(
      accent, [dark[0], dark[1], dark[2]], (1 - dark[3]) * 100);
  const lightText = blendColors(
      accent, [light[0], light[1], light[2]], (1 - light[3]) * 100);
  return contrastRatio(accent, darkText) > contrastRatio(accent, lightText);
}

// ZenGradientGenerator.getPrimaryColor: the middle colour of the theme.
export function dominantColor(theme: SpaceTheme): Rgb|undefined {
  return theme.colors.length ?
      theme.colors[Math.floor(theme.colors.length / 2)] :
      undefined;
}

// ZenGradientGenerator.getAccentColorForUI: as it is on a dark toolbar; on a
// light one more saturated and pulled to Zen's lightness for the window's
// scheme, greys kept (gh-15142).
export function accentColorForUI(
    accent: Rgb, isDark: boolean, windowIsDark: boolean): Rgb {
  const [r, g, b] = accent;
  if (isDark || (r === g && g === b)) {
    return accent;
  }
  const [h, s, l] = rgbToHsl(accent);
  const lightness = l * 0.4 + (windowIsDark ? 0.62 : 0.42) * 0.6;
  return hslToRgb(h / 360, Math.min(1, s + 0.3), lightness);
}

export function themeFromJson(json: string): SpaceTheme {
  try {
    const value = JSON.parse(json);
    const colors: Rgb[] = Array.isArray(value.colors) ?
        value.colors.filter(isRgb).slice(0, MAX_COLORS) :
        [];
    const dots: Dot[] = Array.isArray(value.dots) ?
        value.dots.slice(0, colors.length)
            .filter((d: Dot) => typeof d?.x === 'number' &&
                        typeof d?.y === 'number' && DOT_TYPES.includes(d.type))
            .map((d: Dot) => ({x: d.x, y: d.y, type: d.type})) :
        [];
    return {
      colors,
      // All of them or none: the picker places colours by their dots.
      dots: dots.length === colors.length ? dots : [],
      lightness: Math.min(100, Math.max(0, Number(value.lightness ?? 50) || 0)),
      opacity: Math.min(MAX_OPACITY,
                        Math.max(MIN_OPACITY, Number(value.opacity) || 0.4)),
      texture: Math.min(1, Math.max(0, Number(value.texture) || 0)),
      scheme: ['light', 'dark'].includes(value.scheme) ? value.scheme : 'auto',
      harmony: value.harmony === 'float' ||
              HARMONIES.some(h => h.type === value.harmony) ?
          value.harmony :
          '',
    };
  } catch {
    return {...DEFAULT_THEME};
  }
}
