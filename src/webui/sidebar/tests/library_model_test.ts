// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {arrangeClosed, formatDay, formatUrl, groupByDay, groupBySite, siteOf, startOfDay} from '../library_model.ts';
import type {Visit} from '../library_model.ts';
import {useLocale} from '../../i18n/tests/strings_stub.ts';

const now = new Date(2026, 9, 24, 10, 30).getTime();
const at = (d: number, h = 9) => new Date(2026, 9, d, h).getTime();
const visit = (url: string, time: number, title = ''): Visit =>
    ({url, title, time});

// A zone with daylight saving: Europe's clocks go back on 25 October 2026.
process.env['TZ'] = 'Europe/Berlin';

const DAYS = {
  'en-US': ['Today', 'Yesterday', '3 days ago', '6 days ago', 'Oct 17, 2026', 'Oct 12, 2026',
            'Tomorrow'],
  'en-GB': ['Today', 'Yesterday', '3 days ago', '6 days ago', '17 Oct 2026', '12 Oct 2026',
            'Tomorrow'],
  // Russian plurals come from Intl, not from rules of ours.
  ru: ['Сегодня', 'Вчера', '3 дня назад', '6 дней назад', '17 окт. 2026 г.', '12 окт. 2026 г.',
       'Завтра'],
};

for (const [locale, days] of Object.entries(DAYS)) {
  test(`${locale}: days read relatively up to six days ago, then as a date (Zen)`, () => {
    useLocale(locale);
    try {
      // The last is tomorrow: a clock behind the history's.
      assert.deepEqual([24, 23, 21, 18, 17, 12, 25].map(d => formatDay(startOfDay(at(d)), now)),
                       days);
      // The day the clocks went back has 25 hours, and still is yesterday.
      const afterSwitch = new Date(2026, 9, 26, 10, 30).getTime();
      assert.equal(formatDay(startOfDay(new Date(2026, 9, 25, 9).getTime()), afterSwitch),
                   days[1]);
    } finally {
      useLocale('en-US');
    }
  });
}

test('history groups by day, newest first', () => {
  const groups = groupByDay([
    visit('https://a.test/', at(24, 9)),
    visit('https://b.test/', at(24, 8)),
    visit('https://c.test/', at(22)),
  ]);
  assert.deepEqual(groups.map(g => g.visits.length), [2, 1]);
  assert.equal(groups[0]!.day, startOfDay(at(24)));
});

test('by site: names in order, pages without a site last', () => {
  const groups = groupBySite([
    visit('chrome://settings/', at(24)),
    visit('https://www.zeta.test/x', at(24)),
    visit('https://alpha.test/', at(24)),
    visit('https://zeta.test/y', at(23)),
  ]);
  assert.deepEqual(groups.map(g => g.site), ['alpha.test', 'zeta.test', '']);
  assert.equal(groups[1]!.visits.length, 2);
  assert.equal(siteOf('file:///tmp/a'), '');
});

test('urls lose their scheme, www and the closing slash', () => {
  assert.equal(formatUrl('https://www.github.com/pulls/'), 'github.com/pulls');
  assert.equal(formatUrl('http://example.com'), 'example.com');
});

test('closed tabs read as history: within the time, searched, sorted', () => {
  const tabs = [
    visit('https://old.test/', now - 10 * 86400000, 'Old'),
    visit('https://b.test/', at(24, 8), 'Beta page'),
    visit('https://a.test/', at(24, 9), 'Alpha'),
  ];
  assert.deepEqual(
      arrangeClosed(tabs, 'date', 7, '', now)[0]!.visits.map(v => v.title),
      ['Alpha', 'Beta page']);
  assert.deepEqual(
      arrangeClosed(tabs, 'lastvisited', 30, 'beta', now)[0]!.visits.map(
          v => v.title),
      ['Beta page']);
  assert.equal(arrangeClosed(tabs, 'site', 30, '', now).length, 3);
});
