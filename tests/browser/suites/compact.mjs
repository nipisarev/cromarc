// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Compact mode: the sidebar collapses to nothing, flashes for a background tab,
// and comes back.
import {readFileSync} from 'node:fs';

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

// A tab opened in the background flashes the sidebar over the page (Zen's
// flashSidebar); floating, it gets Zen's opaque toolbar background, which
// the browser says it painted (--cromarc-test-log-background).
// A theme with grain: the floating sidebar keeps it (#zen-toolbar-background
// has a .zen-browser-grain of its own), as the window around it does.
await evaluate(await sidebar(), `(() => {
  const app = document.querySelector('cromarc-sidebar-app');
  const id = app.spaces_.find(s => s.current).id;
  app.proxy_.handler.setSpaceTheme(id, JSON.stringify({colors: [[40, 20, 200]], opacity: 0.5, texture: 0.5}));
  return true;
})()`);
const floatingPainted = () => /cromarc floating sidebar: colors=1 texture=0\.5/.test(readFileSync(process.env.CROMARC_TEST_LOG, 'utf8'));
const browserWs = (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
const openInBackground = () => new Promise(resolve => {
  const ws = new WebSocket(browserWs);
  ws.onopen = () => ws.send(JSON.stringify({id: 1, method: 'Target.createTarget', params: {url: 'data:text/html,<title>later</title>later', background: true}}));
  ws.onmessage = () => { ws.close(); resolve(); };
});
let flashed = false;
for (let i = 0; i < 3 && !flashed; i++) {
  await openInBackground();
  flashed = !!(await until(floatingPainted, 3000));
}
check('flashed over the page, the sidebar has an opaque background with the theme\'s grain', flashed);

await evaluate(await sidebar(), `document.querySelector('cromarc-sidebar-app').proxy_.handler.toggleCompactMode()`);
check('toggled back: the sidebar returns', (await until(async () => (await width()) > 100 && (await pageWidth()) < pageCompact, 10000)) === true,
    `${await width()} ${await pageWidth()}`);
process.exit(results.every(Boolean) ? 0 : 1);
