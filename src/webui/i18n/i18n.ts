// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Cromarc's strings, as Chromium gives them to the page: each controller
// fills loadTimeData from //cromarc/strings in the locale Chromium picked for
// its own UI (webui_strings.h). Nothing here chooses a language.
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

export function s(key: string): string {
  return loadTimeData.getString(key);
}

// $1…$9 in the message take the arguments as they are.
export function sf(key: string, ...args: Array<string|number>): string {
  return loadTimeData.getStringF(key, ...args);
}

// The locale of Chromium's UI ("en-US", "en-GB", "ru"), for Intl.
export function uiLocale(): string {
  return loadTimeData.getString('locale');
}

const formats = new Map<string, object>();

// One Intl formatter per name and locale: building one costs more than using
// it, and rows format on every download tick.
export function cachedFormat<T extends object>(name: string, make: () => T): T {
  const key = `${name} ${uiLocale()}`;
  let format = formats.get(key) as T | undefined;
  if (!format) {
    format = make();
    formats.set(key, format);
  }
  return format;
}
