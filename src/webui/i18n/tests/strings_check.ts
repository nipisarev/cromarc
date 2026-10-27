// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The strings check (AGENTS.md): every message has its Russian, every key a
// message, and src/ has no text of its own and no language of its own.
// Each function takes its inputs, so the tests can hand it broken ones.
import {messageId, messagePlaceholders, placeholdersOf} from './messages.ts';
import type {Message, Translation} from './messages.ts';

export interface Source {
  path: string;
  text: string;
}

function lineOf(text: string, index: number): number {
  return text.slice(0, index).split('\n').length;
}

export function checkTranslations(
    messages: Message[], translations: Translation[]): string[] {
  const problems: string[] = [];
  const byId = new Map<string, Translation[]>();
  for (const t of translations) {
    byId.set(t.id, [...(byId.get(t.id) ?? []), t]);
  }
  const used = new Set<string>();
  for (const message of messages) {
    const id = messageId(message);
    used.add(id);
    const found = byId.get(id);
    if (!found) {
      problems.push(`${message.name} has no ru translation; its id is ${id}`);
      continue;
    }
    if (new Set(found.map(t => t.text)).size > 1) {
      const sharing = messages.filter(m => messageId(m) === id).map(m => m.name);
      problems.push(`${sharing.join(', ')} share one English text but have ` +
                    `different Russian (xtb lines ${found.map(t => t.line).join(', ')}): ` +
                    `one of them needs a meaning`);
      continue;
    }
    const expected = messagePlaceholders(message).join(',');
    const actual = placeholdersOf(found[0]!.text).join(',');
    if (expected !== actual) {
      problems.push(`${message.name}: placeholders {${expected}} in English but ` +
                    `{${actual}} in Russian (xtb line ${found[0]!.line})`);
    }
  }
  for (const t of translations) {
    if (!used.has(t.id)) {
      problems.push(`xtb line ${t.line}: id ${t.id} is no message's (stale)`);
    }
  }
  return problems;
}

const KEY_USE = /\b(?:s|sf)\(\s*'([A-Za-z0-9]+)'|\$i18n\{([A-Za-z0-9]+)\}/g;

export function checkKeys(messages: Message[], sources: Source[]): string[] {
  const problems: string[] = [];
  const keys = new Set(messages.map(m => m.key));
  // Chromium's own loadTimeData keys, and the locale the controllers add.
  const chromium = new Set(['language', 'textdirection', 'fontfamily', 'fontsize', 'locale']);
  const usedKeys = new Set<string>();
  const usedNames = new Set<string>();
  for (const {path, text} of sources) {
    if (/\.(ts|html)$/.test(path)) {
      for (const m of text.matchAll(KEY_USE)) {
        const key = m[1] ?? m[2]!;
        usedKeys.add(key);
        if (!keys.has(key) && !chromium.has(key)) {
          problems.push(`${path}:${lineOf(text, m.index)}: no message for key ${key}`);
        }
      }
    } else {
      for (const m of text.matchAll(/\bIDS_CROMARC_[A-Z0-9_]+/g)) {
        usedNames.add(m[0]);
      }
    }
  }
  for (const message of messages) {
    if (!usedKeys.has(message.key) && !usedNames.has(message.name)) {
      problems.push(`${message.name} (${message.key}) is used nowhere`);
    }
  }
  return problems;
}

// Attributes a person reads, as a literal instead of ${…}.
const LABEL_ATTRIBUTE = /\b(title|aria-label|placeholder|alt)="([^"$]*\p{L}[^"]*)"/gu;
// Text between tags with no ${…} in it; the product's name is not text.
const TEXT_NODE = />([^<>${}`]*\p{L}[^<>${}`]*)</gu;
const NATIVE_TEXT = /\b(SetText|SetTooltipText|SetAccessibleName|SetTitle)\(\s*u"/g;
const OWN_LANGUAGE: Array<[RegExp, string]> = [
  [/new Intl\.\w+\((?!uiLocale\(\))/g, 'an Intl formatter not given uiLocale()'],
  [/\.toLocale(?:Upper|Lower)Case\(\)/g, 'a case change in the process\'s locale'],
  [/\.localeCompare\([^,()]*\)/g, 'a comparison in the process\'s locale'],
  [/\bnavigator\.languages?\b/g, 'navigator.language'],
];
// Chromium's locale reaches the pages from their controllers, and only so.
const CHROMIUM_LOCALE = /\b(AppleLanguages|GetApplicationLocale|kApplicationLocale)\b/g;

export function checkSources(sources: Source[]): string[] {
  const problems: string[] = [];
  for (const {path, text} of sources) {
    for (const m of text.matchAll(/\p{Script=Cyrillic}+/gu)) {
      problems.push(`${path}:${lineOf(text, m.index)}: Cyrillic outside the xtb ` +
                    `("${m[0]}"); its text belongs in cromarc_strings.grd`);
    }
    if (path.endsWith('.html.ts') || path.endsWith('.html')) {
      for (const m of text.matchAll(LABEL_ATTRIBUTE)) {
        problems.push(`${path}:${lineOf(text, m.index)}: hardcoded UI text in ` +
                      `${m[1]}="${m[2]}"`);
      }
      // A page's <title> is only seen where Chromium lists its pages; the
      // ones a person sees use $i18n{} (welcome, Arc import).
      for (const m of text.replace(/<title>[^<]*<\/title>/g, m => ' '.repeat(m.length)).matchAll(TEXT_NODE)) {
        const words = m[1]!.trim();
        if (words && words !== 'Cromarc') {
          problems.push(`${path}:${lineOf(text, m.index)}: hardcoded UI text "${words}"`);
        }
      }
    }
    if (/\.(cc|mm|h)$/.test(path)) {
      for (const m of text.matchAll(NATIVE_TEXT)) {
        problems.push(`${path}:${lineOf(text, m.index)}: hardcoded UI text in ${m[1]}`);
      }
    }
    for (const m of text.matchAll(CHROMIUM_LOCALE)) {
      if (m[1] === 'GetApplicationLocale' && path.endsWith('_ui.cc')) {
        continue;
      }
      problems.push(`${path}:${lineOf(text, m.index)}: ${m[1]}: a language of ` +
                    `our own (only a WebUI controller hands Chromium's locale on)`);
    }
    if (path.endsWith('.ts')) {
      for (const [pattern, what] of OWN_LANGUAGE) {
        for (const m of text.matchAll(pattern)) {
          problems.push(`${path}:${lineOf(text, m.index)}: ${what}`);
        }
      }
    }
  }
  return problems;
}
