// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {cubicBezier, LIBRARY_SPRING, physicalSpring, springFromDuration, THEME_DOT_SPRING} from '../motion.ts';

test('the Library spring overshoots a little and settles, as Zen\'s', () => {
  const spring = physicalSpring(0, 1, LIBRARY_SPRING);
  assert.equal(spring(0).value, 0);
  let peak = 0;
  let settledAt = -1;
  for (let t = 0.004; t < 2; t += 0.004) {
    const {value, done} = spring(t);
    peak = Math.max(peak, value);
    if (done && settledAt < 0) {
      settledAt = t;
    }
  }
  // Damping ratio 0.8: about 1.5% past the end.
  assert.ok(peak > 1.005 && peak < 1.03, String(peak));
  assert.ok(settledAt > 0.2 && settledAt < 0.8, String(settledAt));
  assert.equal(spring(2).value, 1);
});

test('closing runs the same spring the other way', () => {
  const spring = physicalSpring(1, 0, LIBRARY_SPRING);
  assert.ok(spring(0.1).value < 1 && spring(0.1).value > 0);
  assert.equal(spring(3).value, 0);
  assert.ok(spring(3).done);
});

test('cubic-bezier matches CSS at its ends and its middle', () => {
  const ease = cubicBezier(.25, .1, .25, 1);
  assert.equal(ease(0), 0);
  assert.equal(ease(1), 1);
  // CSS "ease" at 50% is about 80% of the way.
  assert.ok(Math.abs(ease(.5) - .8024) < .002, String(ease(.5)));
});

test('a spring by duration has settled to 0.001 of its swing by then', () => {
  const {stiffness, damping, mass} = springFromDuration(0.4, 0.3);
  const zeta = damping / (2 * Math.sqrt(stiffness * mass));
  assert.ok(Math.abs(zeta - 0.7) < 1e-9, String(zeta));
  const omega = Math.sqrt(stiffness / mass);
  const decay = zeta * omega;
  const envelope = decay / (omega * Math.sqrt(1 - zeta * zeta)) * Math.exp(-decay * 0.4);
  assert.ok(Math.abs(envelope - 0.001) < 1e-6, String(envelope));
  assert.deepEqual(THEME_DOT_SPRING, springFromDuration(0.4, 0.3));
  const at = physicalSpring(0, 100, THEME_DOT_SPRING);
  assert.ok(at(0.2).value > 100, 'it overshoots');
  assert.ok(Math.abs(at(0.6).value - 100) < 0.5);
});
