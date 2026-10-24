// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {dayLabel, groupByDay, reasonLabel, timeLabel} from '../library_model.ts';

const now = new Date(2026, 9, 24, 10, 30).getTime();

test('days read as Сегодня, Вчера, then a date', () => {
  assert.equal(dayLabel(new Date(2026, 9, 24, 0, 5).getTime(), now), 'Сегодня');
  assert.equal(dayLabel(new Date(2026, 9, 23, 23, 59).getTime(), now), 'Вчера');
  assert.equal(dayLabel(new Date(2026, 9, 12, 9).getTime(), now), '12 окт.');
  assert.equal(dayLabel(new Date(2025, 11, 31, 9).getTime(), now), '31 дек. 2025');
});

test('the archive groups by day, newest first', () => {
  const tabs = [
    {closedAt: new Date(2026, 9, 24, 9).getTime(), reason: 'idle'},
    {closedAt: new Date(2026, 9, 24, 8).getTime(), reason: 'clear'},
    {closedAt: new Date(2026, 9, 23, 18).getTime(), reason: 'little'},
  ];
  assert.deepEqual(groupByDay(tabs, now).map(g => [g.label, g.tabs.length]),
                   [['Сегодня', 2], ['Вчера', 1]]);
});

test('reasons and times read as in the design', () => {
  assert.equal(reasonLabel('idle'), 'авто, 12 ч простоя');
  assert.equal(reasonLabel('little'), 'Little-окно');
  assert.equal(timeLabel(new Date(2026, 9, 24, 9, 4).getTime()), '09:04');
});
