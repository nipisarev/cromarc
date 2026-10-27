// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {readdirSync, readFileSync} from 'node:fs';
import {join, relative} from 'node:path';
import {test} from 'node:test';
import {fileURLToPath} from 'node:url';

import {messageId, parseGrd, parseXtb, readGrd, readXtb} from './messages.ts';
import type {Message} from './messages.ts';
import {checkKeys, checkSources, checkTranslations} from './strings_check.ts';
import type {Source} from './strings_check.ts';

const SRC = fileURLToPath(new URL('../../../', import.meta.url));

function sourcesOf(dir: string): Source[] {
  const sources: Source[] = [];
  for (const entry of readdirSync(dir, {withFileTypes: true})) {
    const path = join(dir, entry.name);
    const rel = relative(SRC, path);
    if (entry.isDirectory()) {
      if (entry.name !== 'tests' && rel !== join('strings', 'translations')) {
        sources.push(...sourcesOf(path));
      }
    } else if (/\.(ts|html|cc|h|mm)$/.test(entry.name) &&
               !/_(unit|browser)test\.cc$/.test(entry.name)) {
      sources.push({path: `src/${rel}`, text: readFileSync(path, 'utf8')});
    }
  }
  return sources;
}

const grd = (body: string) => `<messages>${body}</messages>`;
const xtb = (body: string) => `<translationbundle lang="ru">\n${body}\n</translationbundle>`;
const translation = (message: Message, text: string) =>
    `<translation id="${messageId(message)}">${text}</translation>`;
const one = (messages: Message[], name: string) => messages.find(m => m.name === name)!;

test('every message has its Russian, under its id, with its placeholders', () => {
  assert.deepEqual(checkTranslations(readGrd(), readXtb()), []);
});

test('every key the pages ask for is a message, and every message is used', () => {
  assert.deepEqual(checkKeys(readGrd(), sourcesOf(SRC)), []);
});

test('src/ has no text and no language of its own', () => {
  assert.deepEqual(checkSources(sourcesOf(SRC)), []);
});

test('N1: a message with no Russian is named, with the id it needs', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_X" desc="">Done</message>'));
  assert.deepEqual(checkTranslations(messages, []),
                   [`IDS_CROMARC_X has no ru translation; its id is ${messageId(messages[0]!)}`]);
});

test('N2: English edited under an old translation: missing and stale', () => {
  const before = parseGrd(grd('<message name="IDS_CROMARC_X" desc="">Close</message>'));
  const after = parseGrd(grd('<message name="IDS_CROMARC_X" desc="">Close all</message>'));
  const problems = checkTranslations(after, parseXtb(xtb(translation(before[0]!, 'Закрыть'))));
  assert.equal(problems.length, 2);
  assert.match(problems[0]!, /^IDS_CROMARC_X has no ru translation/);
  assert.match(problems[1]!, /stale/);
});

test('N3: a placeholder the Russian lacks', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_X" desc="">Always open ' +
      '<ph name="HOST">$1<ex>a.com</ex></ph> in <ph name="SPACE">$2<ex>Work</ex></ph></message>'));
  const problems = checkTranslations(
      messages, parseXtb(xtb(translation(messages[0]!, 'Всегда открывать <ph name="HOST"/>'))));
  assert.equal(problems.length, 1);
  assert.match(problems[0]!, /IDS_CROMARC_X: placeholders \{HOST,SPACE\} in English but \{HOST\}/);
});

test('N4: one English, two Russian: a meaning is needed', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_A" desc="">Today</message>' +
                                '<message name="IDS_CROMARC_B" desc="">Today</message>'));
  const problems = checkTranslations(messages, parseXtb(xtb(
      translation(messages[0]!, 'Today') + '\n' + translation(messages[1]!, 'Сегодня'))));
  assert.equal(problems.length, 2);
  assert.match(problems[0]!, /IDS_CROMARC_A, IDS_CROMARC_B share one English text .* needs a meaning/);
  // ...and a meaning gives each its own id.
  const meant = parseGrd(grd('<message name="IDS_CROMARC_A" desc="">Today</message>' +
                             '<message name="IDS_CROMARC_B" desc="" meaning="A time range">Today</message>'));
  assert.notEqual(messageId(meant[0]!), messageId(meant[1]!));
  assert.deepEqual(checkTranslations(meant, parseXtb(xtb(
      translation(meant[0]!, 'Today') + '\n' + translation(meant[1]!, 'Сегодня')))), []);
});

test('N5: a key no message has, with its place', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_DONE" desc="">Done</message>'));
  const problems = checkKeys(messages, [{path: 'src/webui/a.ts', text: `s('done');\nsf('noSuchKey', 1);`}]);
  assert.deepEqual(problems, ['src/webui/a.ts:2: no message for key noSuchKey']);
});

test('N6: a message nothing uses', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_DONE" desc="">Done</message>' +
                                '<message name="IDS_CROMARC_LITTLE_CLOSE" desc="">Close</message>' +
                                '<message name="IDS_CROMARC_TITLE" desc="">Welcome</message>' +
                                '<message name="IDS_CROMARC_OLD" desc="">Old</message>'));
  const problems = checkKeys(messages, [
    {path: 'src/webui/a.ts', text: `s('done')`},
    {path: 'src/browser/b.cc', text: 'GetStringUTF16(IDS_CROMARC_LITTLE_CLOSE)'},
    {path: 'src/webui/c.html', text: '<title>$i18n{title}</title>'},
  ]);
  assert.deepEqual(problems, ['IDS_CROMARC_OLD (old) is used nowhere']);
});

test('N7: Cyrillic outside the xtb, with its place', () => {
  const problems = checkSources([
    {path: 'src/webui/a.ts', text: `const done = 'Готово';`},
    {path: 'src/browser/b.cc', text: '// fine\nauto s = u"Закрыть";'},
  ]);
  assert.equal(problems.length, 2);
  assert.match(problems[0]!, /^src\/webui\/a\.ts:1: Cyrillic outside the xtb \("Готово"\)/);
  assert.match(problems[1]!, /^src\/browser\/b\.cc:2: Cyrillic/);
});

test('N8: English UI text written into a template or a native view', () => {
  const problems = checkSources([
    {path: 'src/webui/a.html.ts', text: 'html`<button title="Close"><span class="svg"></span></button>`'},
    {path: 'src/webui/b.html.ts', text: 'html`<p>\n  Close\n</p>`'},
    {path: 'src/webui/c.html.ts', text: 'html`<input placeholder="${s(\'x\')}"><h1>Cromarc</h1><b>→ ${a}</b>`'},
    {path: 'src/browser/d.cc', text: 'close->SetTooltipText(u"Close");'},
  ]);
  assert.deepEqual(problems, [
    'src/webui/a.html.ts:1: hardcoded UI text in title="Close"',
    'src/webui/b.html.ts:1: hardcoded UI text "Close"',
    'src/browser/d.cc:1: hardcoded UI text in SetTooltipText',
  ]);
});

test('N9: a language of our own', () => {
  const problems = checkSources([
    {path: 'src/webui/a.ts', text: `new Intl.DateTimeFormat('ru');\nnavigator.language;\n` +
                                   `name.toLocaleUpperCase();\na.localeCompare(b);\n` +
                                   `new Intl.NumberFormat(uiLocale());\na.localeCompare(b, uiLocale());`},
    {path: 'src/browser/b.mm', text: '[defaults objectForKey:@"AppleLanguages"];'},
    {path: 'src/browser/c.cc', text: 'g_browser_process->GetApplicationLocale();'},
    {path: 'src/browser/ui/d_ui.cc', text: 'g_browser_process->GetApplicationLocale();'},
  ]);
  assert.deepEqual(problems.map(p => p.replace(/ \(.*$/, '')), [
    'src/webui/a.ts:1: an Intl formatter not given uiLocale()',
    'src/webui/a.ts:3: a case change in the process\'s locale',
    'src/webui/a.ts:4: a comparison in the process\'s locale',
    'src/webui/a.ts:2: navigator.language',
    'src/browser/b.mm:1: AppleLanguages: a language of our own',
    'src/browser/c.cc:1: GetApplicationLocale: a language of our own',
  ]);
});

test('N10: a translation may put the placeholders in another order', () => {
  const messages = parseGrd(grd('<message name="IDS_CROMARC_X" desc="">Open ' +
      '<ph name="HOST">$1<ex>a.com</ex></ph> in <ph name="SPACE">$2<ex>Work</ex></ph></message>'));
  assert.deepEqual(checkTranslations(messages, parseXtb(xtb(translation(
      messages[0]!, 'В <ph name="SPACE"/> открывать <ph name="HOST"/>')))), []);
});
