// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Today's Clear and its undo toast.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
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
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const sidebar = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const R = `document.querySelector('cromarc-sidebar-app').shadowRoot`;
const pages = async () => (await targets()).filter(t => t.type === 'page' && t.url.startsWith(site)).map(t => t.url).sort();
// The database is locked while Chromium runs: ask the sidebar's handler.
const APP = `document.querySelector('cromarc-sidebar-app')`;
const archived = async () => evaluate(await sidebar(), `${APP}.proxy_.handler.searchArchive('', 50).then(({tabs}) => tabs.map(t => t.url).filter(u => u.startsWith('${site}')))`);

for (const p of ['a.html', 'b.html']) {
  await fetch(`${base}/json/new?${encodeURIComponent(`${site}/${p}`)}`, {method: 'PUT'});
}
const sb = await until(sidebar);
check('setup: two pages in Today', (await until(async () => (await pages()).length === 2 && await evaluate(sb, `${R}.querySelectorAll('.row.today[data-kind="tab"]').length >= 2`))) === true);
const clear = `[...${R}.querySelectorAll('.separator .clear')].find(b => b.textContent.trim() === 'Очистить')`;
await evaluate(sb, `${clear}.click(), true`);
check('Clear closes Today but the last tab', !!(await until(async () => (await pages()).length < 2)), JSON.stringify(await pages()));
check('…the toast offers undo', !!(await until(() => evaluate(sb, `!!${R}.querySelector('#toast button')`))));
check('…and the pages are in the archive', !!(await until(async () => (await archived()).length >= 1, 3000)), JSON.stringify(await archived()));
await evaluate(sb, `${R}.querySelector('#toast button').click(), true`);
check('undo reopens them', !!(await until(async () => (await pages()).length === 2)), JSON.stringify(await pages()));
check('…the toast goes', !!(await until(() => evaluate(sb, `!${R}.querySelector('#toast')`))));
check('…and they leave the archive', !!(await until(async () => (await archived()).length === 0, 4000)), JSON.stringify(await archived()));
await evaluate(sb, `${clear}.click(), true`);
check('without undo the toast goes after 5 s', !!(await until(() => evaluate(sb, `!${R}.querySelector('#toast')`), 7000)));

// Tabs that close later (their beforeunload listeners run first): the window
// still keeps a page instead of closing with the last of them.
for (const p of ['unload.html?1', 'unload.html?2']) {
  await fetch(`${base}/json/new?${encodeURIComponent(`${site}/${p}`)}`, {method: 'PUT'});
}
await until(async () => (await pages()).filter(u => u.includes('unload')).length === 2);
await sleep(1000);
await evaluate(await sidebar(), `${clear}.click(), true`);
await sleep(2500);
check('Clear keeps the window when tabs close slowly', !!(await sidebar()) &&
    (await targets()).filter(t => t.type === 'page').length >= 1,
    JSON.stringify((await targets()).map(t => t.url)));
process.exit(results.every(Boolean) ? 0 : 1);
