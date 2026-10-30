// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {accentColorForUI, blendColors, calculateCompliments, clampToPad, colorFromPosition, contrastRatio, DEFAULT_THEME, dominantColor, gradientOf, harmoniesFor, hslToRgb, initialPosition, LINE_PATH, opacityProgress, PAD, rgbToHsl, SINE_PATH, textureAt, textureDotLit, shouldBeDarkMode, themeFromJson, toolbarColor, wavePath} from '../theme_model.ts';
import type {PickerDot} from '../theme_model.ts';

const near = (a: number, b: number, eps = 1.5) =>
    assert.ok(Math.abs(a - b) <= eps, `${a} ≉ ${b}`);
const center = {x: PAD.width / 2, y: PAD.height / 2};

test('hsl and rgb convert both ways', () => {
  assert.deepEqual(hslToRgb(0, 1, 0.5), [255, 0, 0]);
  assert.deepEqual(hslToRgb(2 / 3, 1, 0.5), [0, 0, 255]);
  const [h, s, l] = rgbToHsl([0, 255, 0]);
  near(h, 120);
  near(s, 1, 0.01);
  near(l, 0.5, 0.01);
});

test('a preset keeps its lightness, saturation grows to the centre', () => {
  // Zen's first preset: data-position 240,240, data-lightness 90.
  const {color, lightness} =
      colorFromPosition({x: 240, y: 240}, 'explicit-lightness', 90);
  assert.deepEqual(color, [244, 237, 215]);
  assert.equal(lightness, 90);
});

test('a float dot is dark at the centre and light at the rim', () => {
  const dark = colorFromPosition({x: 181, y: 180}, 'float', 50);
  assert.deepEqual(dark.color, [0, 0, 0]);
  assert.ok(dark.lightness === 0);
  const light = colorFromPosition({x: 181 + 195, y: 180}, 'float', 50);
  assert.equal(light.lightness, 100);
  const [hue] = rgbToHsl(colorFromPosition({x: 181 + 100, y: 180}, 'float', 50).color);
  near(hue, 0, 1);
});

test('the grey presets have no hue', () => {
  const [r, g, b] =
      colorFromPosition({x: 340, y: 180}, 'explicit-black-white', 50).color;
  assert.ok(r === g && g === b, `${r} ${g} ${b}`);
  assert.equal(r, 207);
});

test('a colour saved without its dot goes back about where it was picked', () => {
  const picked = {x: 250, y: 100};
  const {color} = colorFromPosition(picked, 'float', 50);
  const back = colorFromPosition(initialPosition(color), 'float', 50).color;
  color.forEach((v, i) => near(back[i]!, v, 4));
});

test('dots stay in the circle the pad spans', () => {
  const p = clampToPad({x: 0, y: 0});
  near(Math.hypot(p.x - center.x, p.y - center.y), PAD.width / 2, 0.01);
  assert.deepEqual(clampToPad({x: 100, y: 120}), {x: 100, y: 120});
});

const primary = (x: number, y: number): PickerDot =>
    ({id: 0, position: {x, y}, type: 'float'});

test('adding to one dot gives its complement', () => {
  const {dots, harmony} =
      calculateCompliments([primary(280, center.y)], 1, 'add', 'float');
  assert.equal(harmony, 'complementary');
  assert.equal(dots.length, 2);
  near(dots[1]!.position.x, center.x - 100, 0.01);
  near(dots[1]!.position.y, center.y, 0.01);
});

test('adding to a harmony takes the next one with a dot more', () => {
  const two = calculateCompliments([primary(280, center.y)], 1, 'add', '').dots;
  const {dots, harmony} = calculateCompliments(two, 2, 'add', 'complementary');
  assert.equal(harmony, 'splitComplementary');
  assert.equal(dots.length, 3);
});

test('updating with a harmony puts the others at its angles', () => {
  const dots = [primary(280, center.y), primary(0, 0), primary(0, 0)].map(
      (d, id) => ({...d, id}));
  const result = calculateCompliments(dots, 3, 'update', 'triadic');
  assert.equal(result.harmony, 'triadic');
  const angle = (d: PickerDot) =>
      (Math.atan2(d.position.y - center.y, d.position.x - center.x) * 180 /
           Math.PI + 360) % 360;
  near(angle(result.dots[1]!), 120, 0.01);
  near(angle(result.dots[2]!), 240, 0.01);
});

test('removing leaves the harmony with a dot less, one dot floats', () => {
  const two = [primary(280, center.y), {...primary(0, 0), id: 1}];
  assert.equal(calculateCompliments(two, 2, 'remove', '').harmony, 'complementary');
  const one = [primary(280, center.y)];
  const result = calculateCompliments(one, 1, 'remove', '');
  assert.equal(result.harmony, 'floating');
  assert.deepEqual(result.dots, one);
});

test('a three-colour preset on an empty pad places all three', () => {
  const dots = [primary(236, 111), {...primary(0, 0), id: 1}, {...primary(0, 0), id: 2}];
  const result = calculateCompliments(dots, 0, 'update', 'analogous');
  assert.equal(result.dots.length, 3);
  assert.ok(result.dots.slice(1).every(d => d.position.x !== 0));
});

test('the toggle cycles the harmonies for the number of dots', () => {
  assert.deepEqual(harmoniesFor(2), ['complementary', 'singleAnalogous']);
  assert.deepEqual(harmoniesFor(3), ['splitComplementary', 'analogous', 'triadic']);
});

test('the wave is flat at the start of the range and full at its end', () => {
  assert.equal(wavePath(0), LINE_PATH);
  assert.equal(wavePath(1), SINE_PATH);
  assert.ok(wavePath(0.5).startsWith('M 51.373 27.3475 C'), wavePath(0.5));
  assert.equal(opacityProgress(0.3, false), 0);
  near(opacityProgress(0.55, false), 0.5, 1e-9);
  assert.equal(opacityProgress(0.4, true), 1);
});

test('the grain dial turns clockwise from the top in sixteenths', () => {
  assert.equal(textureAt(0, -10), 0);
  assert.equal(textureAt(10, 0), 0.25);
  assert.equal(textureAt(0, 10), 0.5);
  assert.equal(textureAt(-10, 0), 0.75);
  assert.equal(textureAt(-1, -40), 0);
  const lit = (t: number) =>
      [...Array(16).keys()].filter(i => textureDotLit(i, t));
  assert.deepEqual(lit(0), [12]);
  assert.deepEqual(lit(0.25), [0, 12, 13, 14, 15]);
});

test('the gradient follows Zen for one, two and three colours', () => {
  const base = {...DEFAULT_THEME};
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
  assert.deepEqual(theme.dots, []);
  assert.equal(theme.lightness, 50);
  assert.equal(themeFromJson('nope').colors.length, 0);
  assert.equal(themeFromJson('{"harmony":"triadic"}').harmony, 'triadic');
  assert.equal(themeFromJson('{"harmony":"float"}').harmony, 'float');
  assert.equal(themeFromJson('{"harmony":"bogus"}').harmony, '');
});

test('dots come back with their colours, all or none', () => {
  const json = JSON.stringify({
    colors: [[1, 2, 3], [4, 5, 6]],
    dots: [{x: 10, y: 20, type: 'float'}, {x: 30, y: 40, type: 'explicit-lightness'}],
    lightness: 70,
  });
  const theme = themeFromJson(json);
  assert.deepEqual(theme.dots[1], {x: 30, y: 40, type: 'explicit-lightness'});
  assert.equal(theme.lightness, 70);
  const broken = themeFromJson(JSON.stringify({
    colors: [[1, 2, 3], [4, 5, 6]],
    dots: [{x: 10, y: 20, type: 'float'}, {x: 30, type: 'float'}],
  }));
  assert.deepEqual(broken.dots, []);
});

// ZenGradientGenerator.shouldBeDarkMode / getToolbarColor / getAccentColorForUI.
test('light text on a dark theme, dark text on a light one', () => {
  assert.equal(shouldBeDarkMode([20, 30, 60]), true);
  assert.equal(shouldBeDarkMode([235, 225, 200]), false);
});

test('the bias leans to light text on a middle colour', () => {
  // Mid grey: dark text at Zen's .9 would read better, but the bias counts it
  // .3 fainter, so the toolbar goes dark.
  assert.equal(shouldBeDarkMode([128, 128, 128]), true);
});

test('contrast is symmetric and 21 for black on white', () => {
  assert.equal(Math.round(contrastRatio([0, 0, 0], [255, 255, 255])), 21);
  assert.equal(contrastRatio([10, 20, 30], [200, 210, 220]),
               contrastRatio([200, 210, 220], [10, 20, 30]));
});

test('blending takes the percentage of the first colour', () => {
  assert.deepEqual(blendColors([200, 100, 0], [0, 100, 200], 25), [50, 100, 150]);
});

test('the toolbar text takes a quarter of the accent, at .9', () => {
  assert.deepEqual(toolbarColor(true), [255, 255, 255, 0.9]);
  assert.deepEqual(toolbarColor(false, [100, 100, 100]), [25, 25, 25, 0.9]);
});

test('the dominant colour is the middle one', () => {
  const theme = {...DEFAULT_THEME, colors: [[1, 1, 1], [2, 2, 2], [3, 3, 3]] as Array<[number, number, number]>};
  assert.deepEqual(dominantColor(theme), [2, 2, 2]);
  assert.equal(dominantColor(DEFAULT_THEME), undefined);
});

test('a dark toolbar keeps the accent; a light one saturates it, greys stay', () => {
  assert.deepEqual(accentColorForUI([64, 140, 170], true, false), [64, 140, 170]);
  assert.deepEqual(accentColorForUI([90, 90, 90], false, false), [90, 90, 90]);
  const light = accentColorForUI([64, 140, 170], false, false);
  assert.notDeepEqual(light, [64, 140, 170]);
});
