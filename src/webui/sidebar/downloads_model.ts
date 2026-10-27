// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {cachedFormat, s, sf, uiLocale} from '../i18n/i18n.js';

// What the Library's downloads and the download stack say about a download
// (zen:library/ZenLibraryUtils.sys.mjs, Firefox's DownloadUtils and
// DownloadsCommon strings, in their Russian).

// The fields of sidebar.mojom's Download these read.
export interface DownloadLike {
  fileName: string;
  url: string;
  state: number;
  paused: boolean;
  missing: boolean;
  dangerous: boolean;
  receivedBytes: bigint|number;
  totalBytes: bigint|number;
  speed: bigint|number;
  timeLeft: bigint|number;
  endTime: number;
}

// sidebar.mojom DownloadState.
export const IN_PROGRESS = 0;
export const COMPLETE = 1;
export const CANCELLED = 2;
export const INTERRUPTED = 3;

// ZenLibraryUtils' groups; pictures, video and sound go by extension here,
// as there is no MIME service to ask.
const GROUP_EXTENSIONS: Record<string, string> = {
  images: 'jpg jpeg jpe png gif webp avif bmp ico icns svg heic heif jxl tif ' +
      'tiff cr2 cr3 nef arw dng raf orf rw2',
  video: 'mp4 m4v mov webm mkv avi wmv flv mpg mpeg mpe 3gp 3g2 ogv mts m2ts',
  audio: 'mp3 m4a aac wav flac ogg oga opus aif aiff wma alac',
  documents: 'pdf doc docx xls xlsx ppt pptx txt md rtf odt ods odp csv epub ' +
      'pages numbers key',
  archives: 'zip rar 7z tar gz bz2 xz tgz zst',
  apps: 'dmg pkg exe msi app deb rpm appimage apk jar',
};

// In the order they are offered.
export const FILE_GROUPS = Object.keys(GROUP_EXTENSIONS);

const GROUP_BY_EXTENSION = new Map<string, string>();
for (const [group, extensions] of Object.entries(GROUP_EXTENSIONS)) {
  for (const extension of extensions.split(' ')) {
    GROUP_BY_EXTENSION.set(extension, group);
  }
}

export function extensionOf(fileName: string): string {
  const name = fileName.split(/[/\\]/).pop() ?? '';
  const dot = name.lastIndexOf('.');
  return dot > 0 ? name.slice(dot + 1).toLowerCase() : '';
}

export function fileGroupOf(fileName: string): string|null {
  return GROUP_BY_EXTENSION.get(extensionOf(fileName)) ?? null;
}

const n = (value: bigint|number) => Number(value);

// Still coming in, paused or not.
export function isPending(download: DownloadLike): boolean {
  return download.state === IN_PROGRESS;
}

export function hasProgress(download: DownloadLike): boolean {
  return n(download.totalBytes) > 0;
}

export function progressOf(download: DownloadLike): number {
  return hasProgress(download) ?
      Math.min(100, n(download.receivedBytes) / n(download.totalBytes) * 100) :
      0;
}

// ZenLibraryUtils canRetry: stopped before it was done, not held back.
export function canRetry(download: DownloadLike): boolean {
  return !download.dangerous &&
      (download.state === CANCELLED || download.state === INTERRUPTED);
}

export function hasFile(download: DownloadLike): boolean {
  return download.state === COMPLETE && !download.missing;
}

const oneDecimal = () => cachedFormat(
    'oneDecimal', () => new Intl.NumberFormat(uiLocale(), {maximumFractionDigits: 1}));
const whole = () => cachedFormat(
    'whole', () => new Intl.NumberFormat(uiLocale(), {maximumFractionDigits: 0}));

// DownloadUtils.convertByteUnits: a decimal under 100, none from there.
export function byteUnits(bytes: number): [string, string] {
  const units = [s('unitBytes'), s('unitKilobytes'), s('unitMegabytes'),
                 s('unitGigabytes'), s('unitTerabytes')];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  const format = unit === 0 || value >= 100 ? whole() : oneDecimal();
  return [format.format(value), units[unit]!];
}

// DownloadUtils.convertTimeUnits and Zen's long form: "1m 14s".
export function timeLeftText(seconds: number): string {
  if (seconds < 0) {
    return '';
  }
  const pair = (time: number, unit: string) =>
      sf('timePair', whole().format(time), unit);
  const steps: Array<[number, string]> = [
    [86400, s('unitDays')],
    [3600, s('unitHours')],
    [60, s('unitMinutes')],
    [1, s('unitSeconds')],
  ];
  const index = steps.findIndex(([size]) => seconds >= size);
  if (index < 0) {
    return pair(0, s('unitSeconds'));
  }
  const [size, unit] = steps[index]!;
  const main = Math.floor(seconds / size);
  const next = steps[index + 1];
  const rest = next ? Math.floor((seconds % size) / next[0]) : 0;
  return rest > 0 && next ? `${pair(main, unit)} ${pair(rest, next[1])}` :
                            pair(main, unit);
}

// ZenLibraryUtils transferText: "156 MB/1.1 GB · 3.2 MB/s · 1m 14s".
export function transferText(download: DownloadLike, speed = false): string {
  const [current, currentUnit] = byteUnits(n(download.receivedBytes));
  const rate = speed && !download.paused && n(download.speed) > 0 ?
      sf('downloadSpeed', ...byteUnits(n(download.speed))) :
      '';
  if (!hasProgress(download)) {
    return [`${current} ${currentUnit}`, rate].filter(Boolean).join(' · ');
  }
  const [total, totalUnit] = byteUnits(n(download.totalBytes));
  const transfer = currentUnit === totalUnit ?
      `${current}/${total} ${totalUnit}` :
      `${current} ${currentUnit}/${total} ${totalUnit}`;
  const left = speed && !download.paused ? timeLeftText(n(download.timeLeft)) :
                                           '';
  return [transfer, rate, left].filter(Boolean).join(' · ');
}

// The site, as formatURIForDisplay's onlyBaseDomain gives it.
export function baseDomainOf(url: string): string {
  try {
    const host = new URL(url).hostname.replace(/^www\./, '');
    const parts = host.split('.');
    return parts.length > 2 && !/^\d+$/.test(parts[parts.length - 1]!) ?
        parts.slice(-2).join('.') :
        host;
  } catch {
    return '';
  }
}

const timeOfDay = () => cachedFormat(
    'timeOfDay', () => new Intl.DateTimeFormat(uiLocale(), {timeStyle: 'short'}));
const weekday = () => cachedFormat(
    'weekday', () => new Intl.DateTimeFormat(uiLocale(), {weekday: 'long'}));
const shortDate = () => cachedFormat(
    'shortDate', () => new Intl.DateTimeFormat(uiLocale(), {dateStyle: 'short'}));

function startOfDay(time: number): number {
  const date = new Date(time);
  date.setHours(0, 0, 0, 0);
  return date.getTime();
}

// DownloadUtils.getReadableDates: the time today, "Yesterday", the weekday
// within the week, the date before.
export function readableDate(time: number, now: number): string {
  const days = Math.round((startOfDay(now) - startOfDay(time)) / 86400000);
  if (days <= 0) {
    return timeOfDay().format(time);
  }
  if (days === 1) {
    return s('downloadYesterday');
  }
  if (days < 7) {
    const name = weekday().format(time);
    return name.charAt(0).toLocaleUpperCase(uiLocale()) + name.slice(1);
  }
  return shortDate().format(time);
}

// DownloadsCommon's statusSeparator.
const SEPARATOR = ' — ';

// What a row says under the file's name (ZenLibraryDownloadsSection
// #statusText).
export function statusText(download: DownloadLike, now: number): string {
  if (download.dangerous) {
    return s('downloadDangerous');
  }
  if (isPending(download)) {
    return download.paused ?
        [s('downloadPaused'), transferText(download)].join(SEPARATOR) :
        transferText(download, true);
  }
  if (download.state === COMPLETE) {
    if (download.missing) {
      return s('downloadMissing');
    }
    const [size, unit] = byteUnits(n(download.receivedBytes));
    return [
      `${size} ${unit}`,
      baseDomainOf(download.url),
      download.endTime ? readableDate(download.endTime, now) : '',
    ].filter(Boolean)
        .join(SEPARATOR);
  }
  return download.state === CANCELLED ? s('downloadCanceled') : s('downloadFailed');
}

// ZenLibraryWidget's ENTRIES: the download stack's slots, the newest at the
// bottom, empty ones first.
export const STACK_ENTRIES = 4;

export function stackEntries<T>(downloads: T[]): Array<T|null> {
  const shown = downloads.slice(-STACK_ENTRIES);
  return [...Array(STACK_ENTRIES - shown.length).fill(null), ...shown];
}
