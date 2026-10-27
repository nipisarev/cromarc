// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// /strings.m.js for node tests: what a page's controller puts in
// loadTimeData, read from the grd and the xtb. English unless a test asks for
// another locale.
import {loadTimeData} from '//resources/js/load_time_data.js';

import {stringsFor} from './messages.ts';

export function useLocale(locale: string) {
  loadTimeData.data = {
    ...stringsFor(locale),
    locale,
    language: locale.split('-')[0]!,
    textdirection: 'ltr',
  };
}

useLocale('en-US');
