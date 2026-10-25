// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {addSwipeProgress, circularDiff, easeInOut, isAnimatedStrip, librarySwipeProgress, moveSpace, neighbourIndex, spring, stripTransform, SwipeState} from '../spaces.ts';

const near = (a: number, b: number, eps = 1e-3) =>
    assert.ok(Math.abs(a - b) < eps, `${a} ≉ ${b}`);

test('circular diff takes the short way round, like Zen', () => {
  assert.equal(circularDiff(1, 0, 3), 1);
  assert.equal(circularDiff(2, 0, 3), -1);
  assert.equal(circularDiff(0, 2, 3), 1);
  assert.equal(circularDiff(3, 0, 5), -2);
  assert.equal(circularDiff(0, 0, 1), 0);
});

test('strip transform: index offset ×100% plus half the swipe offset', () => {
  assert.equal(stripTransform(0, 0, 3, 0), 'translateX(0%)');
  assert.equal(stripTransform(1, 0, 3, 0), 'translateX(100%)');
  assert.equal(stripTransform(2, 0, 3, -40), 'translateX(-120%)');
});

test('the neighbour follows the swipe direction', () => {
  assert.equal(neighbourIndex(0, 3, -10), 1);
  assert.equal(neighbourIndex(0, 3, 10), 2);
  assert.equal(neighbourIndex(1, 3, 0), 1);
});

test('swipe updates resist near the edges and freeze past half force', () => {
  const state = new SwipeState(100);
  const first = state.update(0.1, 300);
  near(first, 10 * Math.min(1, 1 - 10 / (300 * 4.5)));
  // A very long drag stops growing once the force multiplier drops to 0.5.
  const s2 = new SwipeState(100);
  let last = 0;
  for (let amount = 0; amount < 20; amount += 0.5) {
    last = s2.update(amount, 300);
  }
  assert.ok(Math.abs(last) < 300 * 4.5, `${last}`);
});

test('spring with bounce 0 settles within its duration and never overshoots', () => {
  const s = spring(0, 100, 0.25);
  assert.equal(s(0), 0);
  let previous = 0;
  for (let t = 0.01; t <= 0.25; t += 0.01) {
    const v = s(t);
    assert.ok(v >= previous - 1e-9, 'monotonic');
    assert.ok(v <= 100 + 1e-9, 'no overshoot');
    previous = v;
  }
  near(s(0.25), 100, 0.2);
  near(s(1), 100, 1e-6);
});

test('only strips between the old and the new space animate', () => {
  // 5 spaces, going from 1 to 3 (right): strips 1, 2 and 3 animate.
  assert.deepEqual([0, 1, 2, 3, 4].map(i => isAnimatedStrip(i, 1, 3, 5)),
                   [false, true, true, true, false]);
  // Going left from 0 to 4 (the short way round): strips 0 and 4.
  assert.deepEqual([0, 1, 2, 3, 4].map(i => isAnimatedStrip(i, 0, 4, 5)),
                   [true, false, false, false, true]);
});

test('dragging a space onto another takes its place', () => {
  assert.deepEqual(moveSpace(['a', 'b', 'c'], 'a', 'c'), ['b', 'c', 'a']);
  assert.deepEqual(moveSpace(['a', 'b', 'c'], 'c', 'a'), ['c', 'a', 'b']);
  assert.deepEqual(moveSpace(['a', 'b', 'c'], 'b', 'c'), ['a', 'c', 'b']);
  assert.equal(moveSpace(['a', 'b'], 'a', 'a'), null);
  assert.equal(moveSpace(['a', 'b'], 'x', 'a'), null);
});

test('the add-space swipe is ready at 80% of a swipe, like Zen', () => {
  near(addSwipeProgress(0.4), 0.5);
  near(addSwipeProgress(-0.8), 1);
  near(addSwipeProgress(5), 1.2);
  near(easeInOut(0), 0);
  near(easeInOut(0.5), 0.5);
  near(easeInOut(1), 1);
});

test('the Library follows 80% of the swipe from where it was, like Zen', () => {
  near(librarySwipeProgress(0, 100, 250), 0.32);
  near(librarySwipeProgress(1, -100, 250), 0.68);
  // Past shut or fully open it only gives a little.
  const past = librarySwipeProgress(1, 250, 250);
  assert.ok(past > 1 && past < 1.2, String(past));
  const before = librarySwipeProgress(0, -250, 250);
  assert.ok(before < 0 && before > -0.2, String(before));
});
