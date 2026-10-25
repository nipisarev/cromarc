// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {colorAt, gradientOf, harmonyPoints, hslToRgb, pointOf, rgbToHsl, themeFromJson} from '../theme_model.ts';

const near = (a: number, b: number, eps = 1.5) =>
    assert.ok(Math.abs(a - b) <= eps, `${a} ≉ ${b}`);

test('hsl and rgb convert both ways', () => {
  assert.deepEqual(hslToRgb(0, 1, 0.5), [255, 0, 0]);
  assert.deepEqual(hslToRgb(2 / 3, 1, 0.5), [0, 0, 255]);
  const [h, s, l] = rgbToHsl([0, 255, 0]);
  near(h, 120);
  near(s, 1, 0.01);
  near(l, 0.5, 0.01);
});

test('on the wheel the angle is the hue and the distance the lightness', () => {
  const size = 200;
  const right = colorAt({x: 190, y: 100}, size);
  const [hue, , lightness] = rgbToHsl(right);
  near(hue, 0, 2);
  near(lightness, 0.9, 0.02);
  const centre = colorAt({x: 100, y: 100}, size);
  assert.deepEqual(centre, [0, 0, 0]);
});

test('a colour goes back to about where it was picked', () => {
  const size = 200;
  const picked = {x: 150, y: 60};
  const back = pointOf(colorAt(picked, size), size);
  near(back.x, picked.x, 3);
  near(back.y, picked.y, 3);
});

test('harmonies place the other dots round the first', () => {
  const size = 200;
  const [, second] = harmonyPoints({x: 180, y: 100}, 2, size);
  near(second!.x, 20, 0.01);
  near(second!.y, 100, 0.01);
  const triad = harmonyPoints({x: 180, y: 100}, 3, size, 'triadic');
  assert.equal(triad.length, 3);
  near(Math.hypot(triad[2]!.x - 100, triad[2]!.y - 100), 80, 0.01);
});

test('the gradient follows Zen for one, two and three colours', () => {
  const base = {opacity: 0.4, texture: 0, scheme: 'auto' as const};
  assert.equal(gradientOf({...base, colors: []}), 'transparent');
  assert.match(gradientOf({...base, colors: [[255, 0, 0]]}), /^rgba\(/);
  assert.equal(
      (gradientOf({...base, colors: [[255, 0, 0], [0, 0, 255]]}).match(/linear-gradient/g) ?? []).length, 2);
  assert.match(
      gradientOf({...base, colors: [[255, 0, 0], [0, 255, 0], [0, 0, 255]]}),
      /radial-gradient.*radial-gradient/);
});

test('stored themes are read defensively', () => {
  const theme = themeFromJson('{"colors":[[1,2,3],[4,5,6],[7,8,9],[1,1,1]],"opacity":5,"scheme":"dark"}');
  assert.equal(theme.colors.length, 3);
  assert.equal(theme.opacity, 0.8);
  assert.equal(theme.scheme, 'dark');
  assert.equal(themeFromJson('nope').colors.length, 0);
  assert.equal(themeFromJson('{"harmony":"triadic"}').harmony, 'triadic');
  assert.equal(themeFromJson('{"harmony":"bogus"}').harmony, '');
});
