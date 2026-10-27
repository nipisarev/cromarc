// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {cachedFormat, uiLocale} from '../i18n/i18n.js';

// What the Library's lists are made of, after zen:library/sections/
// ZenLibraryHistorySection.mjs and ZenLibrarySearchSection.mjs.

export const MS_PER_DAY = 86400000;
// ZenLibrarySearchSection's PAGE_SIZE and WHEN_DAYS; the history section
// reads 120 days back when no time is chosen.
export const PAGE_SIZE = 100;
export const WHEN_DAYS: Record<string, number> = {today: 1, week: 7, month: 30};
export const HISTORY_DAYS_OLD = 120;

// A row of the history section: a page from history, or a closed tab from
// the space's archive (`archiveId`).
export interface Visit {
  url: string;
  title: string;
  time: number;
  archiveId?: bigint;
}

// A titled run of rows: a day (`day`), a site (`site`, '' for pages that
// have none), or none at all.
export interface VisitGroup {
  key: string;
  day?: number;
  site?: string;
  visits: Visit[];
}

export function startOfDay(time: number): number {
  const date = new Date(time);
  date.setHours(0, 0, 0, 0);
  return date.getTime();
}

// Newest first, as they come.
export function groupByDay(visits: Visit[]): VisitGroup[] {
  const groups: VisitGroup[] = [];
  for (const visit of visits) {
    const day = startOfDay(visit.time);
    const last = groups[groups.length - 1];
    if (last && last.day === day) {
      last.visits.push(visit);
    } else {
      groups.push({key: `day-${day}`, day, visits: [visit]});
    }
  }
  return groups;
}

// Firefox's formatURIStringForDisplay for the site of an http(s) page.
export function siteOf(url: string): string {
  try {
    const parsed = new URL(url);
    if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
      return '';
    }
    return parsed.hostname.replace(/^www\./, '');
  } catch {
    return '';
  }
}

// By site name, pages without a site last.
export function groupBySite(visits: Visit[]): VisitGroup[] {
  const bySite = new Map<string, Visit[]>();
  for (const visit of visits) {
    const site = siteOf(visit.url);
    const same = bySite.get(site);
    if (same) {
      same.push(visit);
    } else {
      bySite.set(site, [visit]);
    }
  }
  return [...bySite]
      .sort(([one], [other]) => {
        if (!one || !other) {
          return one ? -1 : 1;
        }
        return one.localeCompare(other, uiLocale());
      })
      .map(([site, visits]) => ({key: `site-${site}`, site, visits}));
}

const relativeDay = () => cachedFormat(
    'relativeDay', () => new Intl.RelativeTimeFormat(uiLocale(), {numeric: 'auto'}));
const mediumDate = () => cachedFormat(
    'mediumDate', () => new Intl.DateTimeFormat(uiLocale(), {dateStyle: 'medium'}));
const visitDate = () => cachedFormat(
    'visitDate',
    () => new Intl.DateTimeFormat(uiLocale(), {dateStyle: 'medium', timeStyle: 'short'}));

// "Today", "Yesterday", "3 days ago" up to six days, then the date.
export function formatDay(day: number, now: number): string {
  const daysAgo = Math.round((startOfDay(now) - day) / MS_PER_DAY);
  const formatted = daysAgo <= 6 ? relativeDay().format(-daysAgo, 'day') :
                                   mediumDate().format(day);
  return formatted.charAt(0).toLocaleUpperCase(uiLocale()) + formatted.slice(1);
}

export function formatVisitTime(time: number): string {
  return visitDate().format(time);
}

export function formatUrl(url: string): string {
  return url.replace(/^https?:\/\/(www\.)?/, '').replace(/\/$/, '');
}

// Closed tabs read the way history is (ZenLibraryHistorySection's
// #closedTabs): within the time, matching the search, sorted.
export function arrangeClosed(
    visits: Visit[], sort: string, daysOld: number, query: string,
    now: number): VisitGroup[] {
  const cutoff = now - daysOld * MS_PER_DAY;
  const needle = query.toLowerCase();
  const shown =
      visits
          .filter(visit => visit.time >= cutoff)
          .filter(
              visit => !needle ||
                  `${visit.title} ${visit.url}`.toLowerCase().includes(needle))
          .sort((a, b) => b.time - a.time);
  switch (sort) {
    case 'site':
      return groupBySite(shown);
    case 'lastvisited':
      return [{key: 'all', visits: shown}];
    default:
      return groupByDay(shown);
  }
}
