// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Chromium in German, which Cromarc has no words for: Chromium's own text is
// German, Cromarc's falls back to English, and days and dates, which Intl
// gives in Chromium's locale, are German.
import {checkCommandBar, checkLibrary, checkLocale, checkNoRawStrings, checkSidebar, checkWelcome, done} from '../language_checks.mjs';

import {ENGLISH} from './language_en.mjs';

await checkLocale({locale: 'de', language: 'de', history: 'Verlauf'});
await checkSidebar(ENGLISH);
await checkWelcome(ENGLISH);
await checkLibrary({...ENGLISH, today: 'Heute'});
await checkCommandBar(ENGLISH);
await checkNoRawStrings();
done();
