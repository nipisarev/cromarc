// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {s, sf, uiLocale} from '../i18n.ts';

import {useLocale} from './strings_stub.ts';

// A space's name is the user's, with whatever is in it.
const NAME = '$1 $$ <b>x</b> «Q» "q" 🙂';

test('arguments go in as they are: not substituted again, not HTML', () => {
  for (const locale of ['en-US', 'ru']) {
    useLocale(locale);
    assert.ok(sf('deleteSpaceBody', NAME).includes(NAME), locale);
    assert.ok(sf('libraryMenuOpen', NAME).includes(NAME), locale);
    assert.equal(sf('littleAlwaysOpen', 'a.com', NAME),
                 locale === 'ru' ? `Всегда открывать a.com в «${NAME}»` :
                                   `Always open a.com in “${NAME}”`);
  }
  useLocale('en-US');
});

test('a translation may take its arguments in another order', () => {
  useLocale('ru');
  loadTimeData.data = {reordered: 'В $2 открывать $1', locale: 'ru'};
  assert.equal(sf('reordered', 'a.com', 'Work'), 'В Work открывать a.com');
  useLocale('en-US');
});

test('the locale is the one Chromium handed the page, nothing else', () => {
  for (const locale of ['en-US', 'en-GB', 'ru', 'de']) {
    useLocale(locale);
    assert.equal(uiLocale(), locale);
  }
  useLocale('en-US');
});

test('another locale reads in English, as grit falls back', () => {
  useLocale('de');
  assert.equal(s('libraryRouting'), 'Routing');
  useLocale('ru');
  assert.equal(s('libraryRouting'), 'Маршруты');
  useLocale('en-US');
});

test('a key with no message throws, as Chromium\'s loadTimeData does', () => {
  assert.throws(() => s('noSuchKey'), /Could not find value for noSuchKey/);
});
