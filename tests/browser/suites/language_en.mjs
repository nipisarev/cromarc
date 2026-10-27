// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Chromium in English (macOS's language for the app: en-US): Cromarc's words
// are English, in Zen's words where they are Zen's.
import {checkCommandBar, checkLibrary, checkLocale, checkNoRawStrings, checkSidebar, checkWelcome, done} from '../language_checks.mjs';

export const ENGLISH = {
  locale: 'en-US',
  language: 'en',
  history: 'History',
  newTab: 'New Tab',
  changeName: 'Change Name…',
  editSpace: 'Edit Space',
  rail: ['Routing', 'Media', 'Downloads', 'Spaces', 'History'],
  today: 'Today',
  bytes: 'bytes',
  placeholder: 'Search or enter address',
  spaceName: 'Personal',
  work: 'Work',
  newFolder: 'New Folder',
};

if (process.argv[1].endsWith('language_en.mjs')) {
  await checkLocale(ENGLISH);
  await checkSidebar(ENGLISH);
  await checkWelcome(ENGLISH);
  await checkLibrary(ENGLISH);
  await checkCommandBar(ENGLISH);
  await checkNoRawStrings();
  done();
}
