// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import type {DotType} from '../sidebar/theme_model.js';

// A preset colour under the pad, as zen-panels/theme-picker.inc writes it:
// where its first dot goes, how many dots, in which harmony, and with which
// lightness; `background` or the three colours `c` only draw the swatch.
export interface Preset {
  position: [number, number];
  numDots: number;
  algo: string;
  lightness?: number;
  type?: DotType;
  background?: string;
  c?: [string, string, string];
}

const float = (lightness: number, x: number, y: number, background: string):
    Preset => ({position: [x, y], numDots: 1, algo: 'float', lightness, background});
const analogous =
    (lightness: number, x: number, y: number, c: [string, string, string]):
        Preset => ({position: [x, y], numDots: 3, algo: 'analogous', lightness, c});
const grey = (x: number, background: string): Preset => ({
  position: [x, 180],
  numDots: 1,
  algo: 'float',
  type: 'explicit-black-white',
  background,
});

// #PanelUI-zen-gradient-generator-color-pages, page by page.
export const PRESET_PAGES: Preset[][] = [
  [
    float(90, 240, 240, '#f4efdf'),
    float(80, 233, 157, '#f0b8cd'),
    float(80, 236, 111, '#e9c3e3'),
    float(70, 234, 173, '#da7682'),
    float(70, 220, 187, '#eb8570'),
    float(60, 225, 237, '#dcce7f'),
    float(60, 147, 195, '#5becad'),
    float(50, 81, 84, '#919bb5'),
  ],
  [
    analogous(90, 240, 240, ['rgb(245, 237, 214)', 'rgb(221, 243, 216)', 'rgb(243, 216, 225)']),
    analogous(85, 233, 157, ['rgb(243, 190, 222)', 'rgb(247, 222, 186)', 'rgb(223, 195, 238)']),
    analogous(80, 236, 111, ['rgb(229, 179, 228)', 'rgb(236, 172, 178)', 'rgb(197, 185, 223)']),
    analogous(70, 234, 173, ['rgb(235, 122, 159)', 'rgb(239, 239, 118)', 'rgb(210, 133, 224)']),
    analogous(70, 220, 187, ['rgb(242, 115, 123)', 'rgb(175, 242, 115)', 'rgb(230, 125, 232)']),
    analogous(60, 225, 237, ['rgb(221, 205, 85)', 'rgb(97, 212, 94)', 'rgb(215, 91, 124)']),
    analogous(60, 147, 195, ['rgb(75, 231, 210)', 'rgb(84, 175, 222)', 'rgb(62, 244, 112)']),
    analogous(55, 81, 84, ['rgb(122, 132, 158)', 'rgb(137, 117, 164)', 'rgb(116, 162, 164)']),
  ],
  [
    float(10, 171, 72, 'rgb(93, 86, 106)'),
    float(40, 265, 79, '#997096'),
    float(35, 301, 176, '#956066'),
    float(30, 237, 210, '#9c6645'),
    float(30, 91, 228, '#517b6c'),
    float(25, 67, 159, '#576e75'),
    float(20, 314, 235, 'rgb(131, 109, 95)'),
    float(20, 118, 215, '#447464'),
  ],
  [
    analogous(10, 171, 72, ['rgb(23, 17, 34)', 'rgb(37, 14, 35)', 'rgb(18, 22, 33)']),
    analogous(40, 265, 79, ['rgb(128, 76, 124)', 'rgb(141, 63, 66)', 'rgb(97, 88, 116)']),
    analogous(35, 301, 176, ['rgb(122, 56, 64)', 'rgb(126, 121, 52)', 'rgb(111, 68, 110)']),
    analogous(30, 237, 210, ['rgb(131, 65, 22)', 'rgb(64, 128, 25)', 'rgb(122, 31, 91)']),
    analogous(30, 91, 228, ['rgb(45, 108, 85)', 'rgb(52, 85, 101)', 'rgb(52, 118, 35)']),
    analogous(25, 67, 159, ['rgb(45, 74, 83)', 'rgb(46, 50, 81)', 'rgb(38, 90, 65)']),
    analogous(20, 314, 235, ['rgb(64, 47, 38)', 'rgb(55, 64, 38)', 'rgb(59, 43, 52)']),
    analogous(20, 118, 215, ['rgb(22, 80, 61)', 'rgb(26, 60, 76)', 'rgb(27, 87, 15)']),
  ],
  // From white to black, along the middle to the right edge.
  [
    grey(340, 'rgb(224, 224, 224)'),
    grey(337.5, 'rgb(224, 224, 224)'),
    grey(315, 'rgb(192, 192, 192)'),
    grey(292.5, 'rgb(160, 160, 160)'),
    grey(270, 'rgb(128, 128, 128)'),
    grey(247.5, 'rgb(96, 96, 96)'),
    grey(225, 'rgb(64, 64, 64)'),
    grey(202.5, 'rgb(32, 32, 32)'),
    grey(180, 'rgb(0, 0, 0)'),
  ],
];

// The swatch as the panel draws it (zen-gradient-generator.css, three-dot
// presets).
export function swatchStyle(preset: Preset): string {
  if (preset.c) {
    const [c1, c2, c3] = preset.c;
    return `background: radial-gradient(circle at 0% 0%, ${c1}, transparent 100%), ` +
        `radial-gradient(circle at 100% 0%, ${c2}, transparent 100%), ` +
        `linear-gradient(to top, ${c3} 0%, transparent 60%)`;
  }
  return `background: ${preset.background}`;
}
