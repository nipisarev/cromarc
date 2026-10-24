// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

export interface ArchivedLike {
  closedAt: number;
  reason: string;
}

export interface DayGroup<T> {
  label: string;
  tabs: T[];
}

const MONTHS = [
  'янв.', 'февр.', 'марта', 'апр.', 'мая', 'июня',
  'июля', 'авг.', 'сент.', 'окт.', 'нояб.', 'дек.',
];

function startOfDay(time: number): number {
  const date = new Date(time);
  date.setHours(0, 0, 0, 0);
  return date.getTime();
}

// "Сегодня", "Вчера", then "12 окт." (with the year when it is not this one).
export function dayLabel(time: number, now: number): string {
  const days = Math.round((startOfDay(now) - startOfDay(time)) / 86400000);
  if (days === 0) {
    return 'Сегодня';
  }
  if (days === 1) {
    return 'Вчера';
  }
  const date = new Date(time);
  const year = date.getFullYear() === new Date(now).getFullYear() ?
      '' :
      ` ${date.getFullYear()}`;
  return `${date.getDate()} ${MONTHS[date.getMonth()]}${year}`;
}

// The archive by day, in the order given (newest first).
export function groupByDay<T extends ArchivedLike>(
    tabs: T[], now: number): Array<DayGroup<T>> {
  const groups: Array<DayGroup<T>> = [];
  for (const tab of tabs) {
    const label = dayLabel(tab.closedAt, now);
    const last = groups[groups.length - 1];
    if (last && last.label === label) {
      last.tabs.push(tab);
    } else {
      groups.push({label, tabs: [tab]});
    }
  }
  return groups;
}

// Why the tab was archived, as the design's archive mock words it.
export function reasonLabel(reason: string): string {
  switch (reason) {
    case 'idle':
      return 'авто, 12 ч простоя';
    case 'clear':
      return '«Очистить»';
    case 'folder':
      return 'папка удалена';
    case 'little':
      return 'Little-окно';
    default:
      return reason;
  }
}

export function timeLabel(time: number): string {
  const date = new Date(time);
  return `${String(date.getHours()).padStart(2, '0')}:${
      String(date.getMinutes()).padStart(2, '0')}`;
}
