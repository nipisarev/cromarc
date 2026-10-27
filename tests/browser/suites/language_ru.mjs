// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Chromium switched from English to Russian between two starts, as macOS
// switches an app: Cromarc's words are today's Russian, and what the person
// made in English (a space's name, a folder, a rule) stays as it was.
import {checkCommandBar, checkDataKept, checkLibrary, checkLocale, checkNoRawStrings, checkSidebar, checkWelcome, done, makeData, phase, restart} from '../language_checks.mjs';

import {ENGLISH} from './language_en.mjs';

if (phase === 1) {
  await checkLocale(ENGLISH);
  await makeData(ENGLISH);
  await restart();
}

// macOS gives this Mac's own variant, ru-ES; Chromium has ru for it.
await checkLocale({locale: 'ru', language: 'ru', history: 'История'});
await checkDataKept(ENGLISH);
await checkSidebar({newTab: 'Новая вкладка', changeName: 'Переименовать', editSpace: 'Изменить пространство'});
// The space keeps the name it was given; the other suggestion is Russian.
await checkWelcome({spaceName: ENGLISH.spaceName, work: 'Работа'});
await checkLibrary({
  rail: ['Маршруты', 'Медиа', 'Загрузки', 'Пространства', 'История'],
  today: 'Сегодня',
  bytes: 'байт',
});
await checkCommandBar({placeholder: 'Поиск или адрес'});
await checkNoRawStrings();
done();
