// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Compact mode: the sidebar collapses to nothing, flashes for a background tab,
// and comes back.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise(r => (ws.onopen = r));
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); }));
  ws.close(); return msg;
}
const evaluate = async (t, expression) => (await send(t, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true})).result?.result?.value;
const sidebar = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const width = async () => evaluate(await sidebar(), 'window.innerWidth');
const page = async () => (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
const pageWidth = async () => evaluate(await page(), 'window.innerWidth');
async function until(fn, ms = 6000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(100); }
  return null;
}
const menu = label => evaluate(null, '');
const clickMenu = async label => evaluate(await sidebar(), `(async () => {
  const app = document.querySelector('cromarc-sidebar-app');
  app.testMenuChoice = ${JSON.stringify(label)};
  app.onBackgroundContextmenu_(new MouseEvent('contextmenu', {clientX: 20, clientY: 300}));
  await app.updateComplete;
  return app.testMenuChoice === null;
})()`);

const before = await width();
const pageBefore = await pageWidth();
console.log('sidebar', before, 'page', pageBefore);
check('the sidebar has width', before > 100);
check('menu has "Скрыть сайдбар"', await clickMenu('Скрыть сайдбар'));
check('collapsed: the sidebar is gone', (await until(async () => (await width()) <= 2)) === true, String(await width()));
const pageCompact = await until(async () => { const w = await pageWidth(); return w > pageBefore && w; });
console.log('page in compact', pageCompact);
check('the page takes the window', pageCompact >= pageBefore + before - 2, `${pageCompact} vs ${pageBefore}+${before}`);

await evaluate(await sidebar(), `document.querySelector('cromarc-sidebar-app').proxy_.handler.toggleCompactMode()`);
check('toggled back: the sidebar returns', (await until(async () => (await width()) > 100 && (await pageWidth()) < pageCompact, 10000)) === true,
    `${await width()} ${await pageWidth()}`);
process.exit(results.every(Boolean) ? 0 : 1);
