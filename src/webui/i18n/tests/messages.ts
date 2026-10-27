// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Reads //cromarc/strings the way grit does: the grd's messages, their ids
// (grit/extern/tclib.py) and the xtb's translations.
import {createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';

export const GRD = new URL('../../../strings/cromarc_strings.grd', import.meta.url);
export const XTB =
    new URL('../../../strings/translations/cromarc_strings_ru.xtb', import.meta.url);

type Part = string|{ph: string, text: string};

export interface Message {
  name: string;
  key: string;
  meaning: string;
  parts: Part[];
}

export interface Translation {
  id: string;
  // The text with each placeholder as {NAME}.
  text: string;
  line: number;
}

function unescapeXml(text: string): string {
  return text.replace(/&(lt|gt|quot|apos|amp|#(\d+)|#x([0-9a-f]+));/gi,
                      (_, name: string, dec?: string, hex?: string) => {
                        if (dec) {
                          return String.fromCodePoint(Number(dec));
                        }
                        if (hex) {
                          return String.fromCodePoint(parseInt(hex, 16));
                        }
                        return ({lt: '<', gt: '>', quot: '"', apos: '\'', amp: '&'} as
                                Record<string, string>)[name]!;
                      });
}

function attributes(text: string): Record<string, string> {
  const result: Record<string, string> = {};
  for (const [, name, value] of text.matchAll(/([a-z_]+)="([^"]*)"/g)) {
    result[name!] = unescapeXml(value!);
  }
  return result;
}

// IDS_CROMARC_NEW_TAB is newTab (webui_strings.cc's WebUIStringKey).
export function keyOf(name: string): string {
  return name.replace(/^IDS_CROMARC_/, '')
      .toLowerCase()
      .replace(/_([a-z0-9])/g, (_, c: string) => c.toUpperCase());
}

export function parseGrd(xml: string): Message[] {
  const messages: Message[] = [];
  for (const [, attrs, body] of
           xml.matchAll(/<message\s([^>]*)>([\s\S]*?)<\/message>/g)) {
    const {name = '', meaning = ''} = attributes(attrs!);
    const parts: Part[] = [];
    let last = 0;
    const text = body!.trim();
    for (const ph of text.matchAll(/<ph name="([A-Z0-9_]+)">([\s\S]*?)<\/ph>/g)) {
      parts.push(unescapeXml(text.slice(last, ph.index)));
      parts.push({ph: ph[1]!, text: unescapeXml(ph[2]!.replace(/<ex>[\s\S]*?<\/ex>/, ''))});
      last = ph.index + ph[0].length;
    }
    parts.push(unescapeXml(text.slice(last)));
    messages.push({name, key: keyOf(name), meaning, parts: parts.filter(p => p !== '')});
  }
  return messages;
}

// What the message reads in the English pak: placeholders become $1….
export function englishText(message: Message): string {
  return message.parts.map(p => typeof p === 'string' ? p : p.text).join('');
}

// The text grit hashes: placeholders by name.
export function presentableText(message: Message): string {
  return message.parts.map(p => typeof p === 'string' ? p : p.ph).join('');
}

export function placeholdersOf(text: string): string[] {
  return [...text.matchAll(/\{([A-Z0-9_]+)\}/g)].map(m => m[1]!).sort();
}

export function messagePlaceholders(message: Message): string[] {
  return message.parts.flatMap(p => typeof p === 'string' ? [] : [p.ph]).sort();
}

function fingerprint(text: string): bigint {
  const hex = createHash('md5').update(text, 'utf8').digest('hex').slice(0, 16);
  let fp = BigInt(`0x${hex}`);
  if (fp & 0x8000000000000000n) {
    fp = -((~fp & 0xFFFFFFFFFFFFFFFFn) + 1n);
  }
  return fp;
}

// grit's GenerateMessageId.
export function messageId(message: Message): string {
  let fp = fingerprint(presentableText(message));
  if (message.meaning) {
    const meaning = fingerprint(message.meaning);
    fp = fp < 0n ? meaning + (fp << 1n) + 1n : meaning + (fp << 1n);
  }
  return (fp & 0x7FFFFFFFFFFFFFFFn).toString();
}

export function parseXtb(xml: string): Translation[] {
  const translations: Translation[] = [];
  for (const m of xml.matchAll(/<translation id="(\d+)">([\s\S]*?)<\/translation>/g)) {
    translations.push({
      id: m[1]!,
      text: unescapeXml(m[2]!.replace(/<ph name="([A-Z0-9_]+)"\s*\/>/g, '{$1}')),
      line: xml.slice(0, m.index).split('\n').length,
    });
  }
  return translations;
}

export function readGrd(): Message[] {
  return parseGrd(readFileSync(GRD, 'utf8'));
}

export function readXtb(): Translation[] {
  return parseXtb(readFileSync(XTB, 'utf8'));
}

// Each key's text in |locale|, English where there is no translation, as
// grit's fallback_to_english gives it.
export function stringsFor(locale: string): Record<string, string> {
  const messages = readGrd();
  const russian = new Map<string, string>();
  if (locale.split('-')[0] === 'ru') {
    for (const t of readXtb()) {
      russian.set(t.id, t.text);
    }
  }
  const result: Record<string, string> = {};
  for (const message of messages) {
    const translated = russian.get(messageId(message));
    if (translated === undefined) {
      result[message.key] = englishText(message);
      continue;
    }
    const byName = new Map(message.parts.flatMap(
        p => typeof p === 'string' ? [] : [[p.ph, p.text] as const]));
    result[message.key] =
        translated.replace(/\{([A-Z0-9_]+)\}/g, (_, name: string) => byName.get(name)!);
  }
  return result;
}
