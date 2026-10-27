// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {baseDomainOf, byteUnits, canRetry, CANCELLED, COMPLETE, fileGroupOf, IN_PROGRESS, INTERRUPTED, readableDate, stackEntries, statusText, timeLeftText, transferText} from '../downloads_model.ts';
import type {DownloadLike} from '../downloads_model.ts';
import {useLocale} from '../../i18n/tests/strings_stub.ts';

const now = new Date(2026, 9, 24, 15, 30).getTime();
const download = (fields: Partial<DownloadLike>): DownloadLike => ({
  fileName: 'file.zip',
  url: 'https://downloads.example.com/file.zip',
  state: COMPLETE,
  paused: false,
  missing: false,
  dangerous: false,
  receivedBytes: 0,
  totalBytes: -1,
  speed: 0,
  timeLeft: -1,
  endTime: 0,
  ...fields,
});

test('files fall in Zen\'s groups by extension', () => {
  assert.equal(fileGroupOf('Photo.HEIC'), 'images');
  assert.equal(fileGroupOf('talk.mov'), 'video');
  assert.equal(fileGroupOf('report.pdf'), 'documents');
  assert.equal(fileGroupOf('backup.tar.gz'), 'archives');
  assert.equal(fileGroupOf('Installer.dmg'), 'apps');
  assert.equal(fileGroupOf('README'), null);
});

// In Chromium's English and in its Russian (today's words, unchanged).
const LOCALES = ['en-US', 'ru'] as const;
const inLocale = (locale: string, fn: () => void) => {
  useLocale(locale);
  try {
    fn();
  } finally {
    useLocale('en-US');
  }
};
const expect = (locale: string, values: {'en-US': unknown, ru: unknown}) =>
    values[locale as 'en-US'|'ru'];

for (const locale of LOCALES) {
  test(`${locale}: sizes keep a decimal under 100, as Firefox does`, () => inLocale(locale, () => {
    const sizes = [0, 1, 512, 1023, 1024, 1536, 156 * 1024 * 1024, 1.07 * 1024 ** 3,
                   2 * 1024 ** 4].map(bytes => byteUnits(bytes).join(' '));
    assert.deepEqual(sizes, expect(locale, {
      'en-US': ['0 bytes', '1 bytes', '512 bytes', '1,023 bytes', '1 KB', '1.5 KB', '156 MB',
                '1.1 GB', '2 TB'],
      ru: ['0 байт', '1 байт', '512 байт', '1\u00a0023 байт', '1 КБ', '1,5 КБ', '156 МБ',
           '1,1 ГБ', '2 ТБ'],
    }));
  }));

  test(`${locale}: time left reads like Zen's long form`, () => inLocale(locale, () => {
    assert.deepEqual([-1, 0, 59, 74, 3600, 90000].map(timeLeftText), expect(locale, {
      'en-US': ['', '0s', '59s', '1m 14s', '1h', '1d 1h'],
      ru: ['', '0 с', '59 с', '1 мин 14 с', '1 ч', '1 дн 1 ч'],
    }));
  }));

  test(`${locale}: a download coming in shows how much, how fast and how long`,
       () => inLocale(locale, () => {
         const coming = download({
           state: IN_PROGRESS,
           receivedBytes: 156 * 1024 * 1024,
           totalBytes: 1.07 * 1024 ** 3,
           speed: 3.2 * 1024 * 1024,
           timeLeft: 74,
         });
         assert.equal(transferText(coming, true), expect(locale, {
           'en-US': '156 MB/1.1 GB · 3.2 MB/s · 1m 14s',
           ru: '156 МБ/1,1 ГБ · 3,2 МБ/с · 1 мин 14 с',
         }));
         // Paused, or with no speed yet: no speed and no time left.
         assert.equal(statusText({...coming, paused: true}, now), expect(locale, {
           'en-US': 'Paused — 156 MB/1.1 GB',
           ru: 'Приостановлено — 156 МБ/1,1 ГБ',
         }));
         assert.equal(transferText({...coming, speed: 0, timeLeft: -1}, true),
                      expect(locale, {'en-US': '156 MB/1.1 GB', ru: '156 МБ/1,1 ГБ'}));
       }));

  test(`${locale}: a finished one gives its size, its site and when`,
       () => inLocale(locale, () => {
         const time = (date: Date, options: Intl.DateTimeFormatOptions) =>
             new Intl.DateTimeFormat(locale, options).format(date);
         const lastHour = new Date(now - 3600000);
         assert.equal(statusText(download({receivedBytes: 2048, endTime: lastHour.getTime()}), now),
                      expect(locale, {
                        'en-US': `2 KB — example.com — ${time(lastHour, {timeStyle: 'short'})}`,
                        ru: '2 КБ — example.com — 14:30',
                      }));
         assert.equal(readableDate(new Date(2026, 9, 23, 9).getTime(), now),
                      expect(locale, {'en-US': 'Yesterday', ru: 'Вчера'}));
         assert.equal(readableDate(new Date(2026, 9, 20, 9).getTime(), now),
                      expect(locale, {'en-US': 'Tuesday', ru: 'Вторник'}));
         assert.equal(readableDate(new Date(2026, 9, 2, 9).getTime(), now),
                      expect(locale, {'en-US': '10/2/26', ru: '02.10.2026'}));
       }));

  test(`${locale}: what a stopped download says`, () => inLocale(locale, () => {
    assert.deepEqual([
      statusText(download({missing: true}), now),
      statusText(download({state: CANCELLED}), now),
      statusText(download({state: INTERRUPTED}), now),
      statusText(download({dangerous: true}), now),
    ], expect(locale, {
      'en-US': ['File moved or missing', 'Canceled', 'Failed', 'This file may be dangerous'],
      ru: ['Файл перемещён или отсутствует', 'Отменено', 'Ошибка', 'Этот файл может быть опасен'],
    }));
  }));
}

test('only a stopped download that was not held back can be retried', () => {
  assert.ok(canRetry(download({state: CANCELLED})));
  assert.ok(canRetry(download({state: INTERRUPTED})));
  assert.ok(!canRetry(download({state: INTERRUPTED, dangerous: true})));
  assert.ok(!canRetry(download({state: COMPLETE})));
  assert.equal(baseDomainOf('https://www.dl.github.com/x'), 'github.com');
});

test('the download stack shows the last four, newest at the bottom', () => {
  assert.deepEqual(stackEntries([1, 2]), [null, null, 1, 2]);
  assert.deepEqual(stackEntries([1, 2, 3, 4, 5, 6]), [3, 4, 5, 6]);
});
