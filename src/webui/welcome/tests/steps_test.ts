// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {cssColor, nextStep, stepsFor} from '../steps.ts';

test('the Arc step is there only when Arc is', () => {
  assert.ok(stepsFor(true).includes('arc'));
  assert.ok(!stepsFor(false).includes('arc'));
});

test('steps go forward and stop at done', () => {
  const steps = stepsFor(false);
  assert.equal(nextStep(steps, 'more'), 'default');
  assert.equal(nextStep(steps, 'done'), 'done');
});

test('colours read as CSS', () => {
  assert.equal(cssColor(0xFF6B40D9), '#6b40d9');
  assert.equal(cssColor(0xFF00000A), '#00000a');
});
