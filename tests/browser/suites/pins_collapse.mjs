// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Zen's collapsible pins: a click on the space header folds the pinned tabs
// but the selected one, the chevron stands in for the icon, the state
// survives a reload, and pinning a tab unfolds them.
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
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => {
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); };
  });
  ws.close();
  return msg;
}
async function evaluate(target, expression) {
  const m = await send(target, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  if (m.result?.exceptionDetails) throw new Error(JSON.stringify(m.result.exceptionDetails).slice(0, 300));
  return m.result?.result?.value;
}
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const sidebar = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const R = `${APP}.shadowRoot`;
const pinnedTitles = `[...${R}.querySelectorAll('#tree .row.pinned .title')].map(e => e.textContent)`;
const todayTab = title => `${APP}.tabs_.find(t => t.title === '${title}' && !${APP}.view_.rows.some(r => r.tab?.id === t.id))?.id`;

for (const t of ['Alpha', 'Beta', 'Gamma']) {
  await fetch(`${base}/json/new?${encodeURIComponent(`data:text/html,<title>${t}</title>${t}`)}`, {method: 'PUT'});
}
let sb = await until(sidebar);
for (const t of ['Alpha', 'Beta', 'Gamma']) {
  await until(() => evaluate(sb, todayTab(t)));
}
for (const t of ['Alpha', 'Beta']) {
  const id = await evaluate(sb, todayTab(t));
  await evaluate(sb, `${APP}.proxy_.handler.pinTab('${id}', '', 99, false), true`);
}
check('setup: two pinned tabs', !!(await until(async () => JSON.stringify(await evaluate(sb, pinnedTitles)) === '["Alpha","Beta"]')),
    JSON.stringify(await evaluate(sb, pinnedTitles)));

// Select Beta, then fold.
await evaluate(sb, `[...${R}.querySelectorAll('#tree .row.pinned')].find(r => r.textContent.includes('Beta')).click(), true`);
await until(() => evaluate(sb, `!!${R}.querySelector('#tree .row.pinned[active]')`));
await evaluate(sb, `${R}.querySelector('.space-header .space-name').click(), true`);
check('a click on the header folds the pinned tabs but the selected one',
    !!(await until(async () => JSON.stringify(await evaluate(sb, pinnedTitles)) === '["Beta"]')),
    JSON.stringify(await evaluate(sb, pinnedTitles)));
check('…the chevron stands in for the icon', await evaluate(sb,
    `${R}.querySelector('.space-header').hasAttribute('collapsed') && getComputedStyle(${R}.querySelector('.space-header .chevron')).display !== 'none'`));

await send(sb, 'Page.reload');
await sleep(1500);
sb = await until(sidebar);
check('folded after a reload', !!(await until(async () => JSON.stringify(await evaluate(sb, pinnedTitles)) === '["Beta"]')),
    JSON.stringify(await evaluate(sb, pinnedTitles)));

const gamma = await until(() => evaluate(sb, todayTab('Gamma')));
await evaluate(sb, `${APP}.proxy_.handler.pinTab('${gamma}', '', 99, false), true`);
check('pinning a tab unfolds them', !!(await until(async () => (await evaluate(sb, pinnedTitles)).length === 3)),
    JSON.stringify(await evaluate(sb, pinnedTitles)));

await evaluate(sb, `${R}.querySelector('.space-header .space-name').click(), true`);
await evaluate(sb, `${R}.querySelector('.space-header .space-name').click(), true`);
check('two clicks: open again', !!(await until(async () => (await evaluate(sb, pinnedTitles)).length === 3)));
process.exit(results.every(Boolean) ? 0 : 1);
