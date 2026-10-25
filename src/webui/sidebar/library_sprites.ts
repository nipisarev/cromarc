// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {BrowserProxy} from './browser_proxy.js';

// The Library's animated section icons, Zen's own (MPL) 36-frame sprites.
// Firefox paints them with context-fill and context-stroke from CSS; an
// image here has no such thing, so the colours go into the SVG itself.
const sources = new Map<string, Promise<string>>();
const urls = new Map<string, string>();
// Who asked for a URL still loading.
const waiting = new Map<string, Array<() => void>>();

function source(id: string): Promise<string> {
  let text = sources.get(id);
  if (!text) {
    text = BrowserProxy.getInstance()
               .handler.getLibrarySprite(id)
               .then(({svg}) => svg);
    sources.set(id, text);
  }
  return text;
}

// A CSS url() of sprite `id` drawn with these colours, or '' until it has
// loaded (then `onLoad` runs, for everyone who asked meanwhile).
export function spriteUrl(
    id: string, stroke: string, fill: string, onLoad: () => void): string {
  const key = `${id}|${stroke}|${fill}`;
  const cached = urls.get(key);
  if (cached) {
    return cached;
  }
  const waiters = waiting.get(key);
  if (waiters) {
    waiters.push(onLoad);
    return '';
  }
  waiting.set(key, [onLoad]);
  source(id).then(text => {
    const done = waiting.get(key) ?? [];
    waiting.delete(key);
    if (!text) {
      return;
    }
    const svg = text.replaceAll('context-stroke', stroke)
                    .replaceAll('context-fill', fill);
    urls.set(key, `url("data:image/svg+xml;base64,${btoa(svg)}")`);
    done.forEach(waiter => waiter());
  });
  return '';
}
