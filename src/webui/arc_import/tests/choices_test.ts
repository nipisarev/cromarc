// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {defaultTargets} from '../choices.ts';

const NEW = 'new';
const targets = [
  {id: 'Default', name: 'Personal', current: true},
  {id: 'Profile 1', name: 'Work', current: false},
];

test('same names pair up, ignoring case and spaces', () => {
  const spaces = [{arcId: 'a', title: 'work '}, {arcId: 'b', title: 'Personal'}];
  assert.deepEqual(
      defaultTargets(spaces, targets, NEW), {a: 'Profile 1', b: 'Default'});
});

test('the first unmatched Arc space goes to the current space', () => {
  const spaces = [{arcId: 'a', title: 'Home'}, {arcId: 'b', title: 'Side'}];
  assert.deepEqual(defaultTargets(spaces, targets, NEW), {a: 'Default', b: NEW});
});

test('a space taken by name is not reused for the unmatched', () => {
  const spaces = [{arcId: 'a', title: 'Home'}, {arcId: 'b', title: 'Personal'}];
  assert.deepEqual(defaultTargets(spaces, targets, NEW), {a: NEW, b: 'Default'});
});
