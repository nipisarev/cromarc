// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {baseDomainOf, byteUnits, canRetry, CANCELLED, COMPLETE, fileGroupOf, IN_PROGRESS, INTERRUPTED, readableDate, statusText, timeLeftText, transferText} from '../downloads_model.ts';
import type {DownloadLike} from '../downloads_model.ts';

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

test('sizes keep a decimal under 100, as Firefox does', () => {
  assert.deepEqual(byteUnits(512), ['512', 'байт']);
  assert.deepEqual(byteUnits(1536), ['1,5', 'КБ']);
  assert.deepEqual(byteUnits(156 * 1024 * 1024), ['156', 'МБ']);
  assert.deepEqual(byteUnits(1.07 * 1024 ** 3), ['1,1', 'ГБ']);
});

test('time left reads like Zen\'s long form', () => {
  assert.equal(timeLeftText(74), '1 мин 14 с');
  assert.equal(timeLeftText(3600), '1 ч');
  assert.equal(timeLeftText(-1), '');
});

test('a download coming in shows how much, how fast and how long', () => {
  const coming = download({
    state: IN_PROGRESS,
    receivedBytes: 156 * 1024 * 1024,
    totalBytes: 1.07 * 1024 ** 3,
    speed: 3.2 * 1024 * 1024,
    timeLeft: 74,
  });
  assert.equal(transferText(coming, true), '156 МБ/1,1 ГБ · 3,2 МБ/с · 1 мин 14 с');
  assert.equal(statusText({...coming, paused: true}, now),
               'Приостановлено — 156 МБ/1,1 ГБ');
});

test('a finished one gives its size, its site and when', () => {
  assert.equal(
      statusText(download({receivedBytes: 2048, endTime: now - 3600000}), now),
      '2 КБ — example.com — 14:30');
  assert.equal(statusText(download({missing: true}), now),
               'Файл перемещён или отсутствует');
  assert.equal(readableDate(new Date(2026, 9, 23, 9).getTime(), now), 'Вчера');
});

test('only a stopped download that was not held back can be retried', () => {
  assert.ok(canRetry(download({state: CANCELLED})));
  assert.ok(canRetry(download({state: INTERRUPTED})));
  assert.ok(!canRetry(download({state: INTERRUPTED, dangerous: true})));
  assert.ok(!canRetry(download({state: COMPLETE})));
  assert.equal(baseDomainOf('https://www.dl.github.com/x'), 'github.com');
});
