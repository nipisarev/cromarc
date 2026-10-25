// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Library's Media (zen:library/sections/ZenLibraryMediaSection.mjs):
// nothing is looked at until agreed to; then the pictures of the folders,
// QuickLook's thumbnails, a preview that steps through them. The folders are
// a fixture, never the Mac's own Downloads, Documents or Pictures.
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';

const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const fixture = join(dirname(fileURLToPath(import.meta.url)), '..', 'fixtures', 'media');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise(r => (ws.onopen = r));
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); }));
  ws.close(); return msg;
}
const evaluate = async (t, expression) => {
  const m = await send(t, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  if (m.result?.exceptionDetails) throw new Error(JSON.stringify(m.result.exceptionDetails).slice(0, 300));
  return m.result?.result?.value;
};
async function until(fn, ms = 10000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const sb = (await (await fetch(`${base}/json/list`)).json()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const LIB = `${APP}.shadowRoot.querySelector('cromarc-library')`;
const IN = selector => `${LIB}.shadowRoot.querySelector(${JSON.stringify(selector)})`;
const ALL = selector => `[...${LIB}.shadowRoot.querySelectorAll(${JSON.stringify(selector)})]`;
const M = 'cromarc-library-media';
const names = () => evaluate(sb, `${ALL(`${M} .media-item`)}.map(n => n.title).sort()`);

// The fixture folder before anything could look at the real ones.
await until(() => evaluate(sb, `!!${APP}.shadowRoot.querySelector('#library-button')`));
await evaluate(sb, `localStorage.setItem('cromarc.library.media.folders', ${JSON.stringify(fixture)}), localStorage.removeItem('cromarc.library.media.enabled'), true`);
await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
await until(() => evaluate(sb, `${APP}.libraryProgress_ === 1`));
await evaluate(sb, `${IN('.tab[data-section="media"]')}.click(), true`);
check('Media asks first, over empty tiles', !!(await until(() => evaluate(sb,
    `!!${IN(`${M} .media-opt-in`)} && !!${IN(`${M} .media-skeleton`)} && !${IN(`${M} .media-grid`)}`))));

await evaluate(sb, `${IN(`${M} .media-opt-in-button`)}.click(), true`);
check('agreeing shows the pictures of the folders, two deep', JSON.stringify(await until(async () => {
  const n = await names();
  return n.length === 2 && n;
})) === '["sea.png","sunset.png"]', JSON.stringify(await names()));
check('…with QuickLook\'s thumbnails', !!(await until(() => evaluate(sb,
    `${ALL(`${M} .media-thumb`)}.every(i => i.complete && i.naturalWidth > 0)`))),
    JSON.stringify(await evaluate(sb, `${ALL(`${M} .media-thumb`)}.map(i => i.src + ' ' + i.naturalWidth)`)));

await evaluate(sb, `${ALL(`${M} .media-item`)}[0].click(), true`);
const first = await until(() => evaluate(sb, `${IN(`${M} .media-preview-name`)}?.textContent`));
check('a click opens it big', !!first);
check('…the Library taking what the column has', !!(await until(async () =>
    (await evaluate(sb, `${APP}.libraryWidth_`)) > 418)));
check('…the picture itself', !!(await until(() => evaluate(sb,
    `(() => { const i = ${IN(`${M} .media-preview-media`)}; return i.src.includes('media-file/') && i.complete && i.naturalWidth > 0; })()`))));
await evaluate(sb, `document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}))`);
const second = await until(async () => { const n = await evaluate(sb, `${IN(`${M} .media-preview-name`)}.textContent`); return n !== first && n; });
check('the right arrow steps to the next one', !!second, String(second));
await evaluate(sb, `document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}))`);
check('Escape closes the preview, the Library back to 418px', !!(await until(async () =>
    !(await evaluate(sb, `!!${IN(`${M} .media-preview`)}`)) && (await evaluate(sb, `${APP}.libraryWidth_`)) === 418)));
check('…and leaves the Library open', await evaluate(sb, `${APP}.libraryOpen_`) === true);

await evaluate(sb, `${APP}.testMenuChoice = 'Выключить медиа'; ${IN('.tab[data-section="media"]')}.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, cancelable: true})), true`);
check('the tab\'s menu turns Media off again', !!(await until(() => evaluate(sb, `!!${IN(`${M} .media-opt-in`)}`))));
process.exit(results.every(Boolean) ? 0 : 1);
