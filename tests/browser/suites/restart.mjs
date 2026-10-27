// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// After a restart a pinned tab is asleep, as Zen's pinned tabs come back
// unloaded: the tab the session restores for it is closed, and a click wakes
// it into one tab. The runner restarts the browser (tooling/test-browser).
const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
const phase = Number(process.env.CROMARC_TEST_PHASE ?? 1);
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
async function until(fn, ms = 10000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };
const APP = `document.querySelector('cromarc-sidebar-app')`;
const ROOT = `${APP}.shadowRoot`;
const todayUrl = `${site}/icon.html?restart-today`;
const pinUrl = `${site}/icon.html?restart-pin`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const pinPages = async () => (await targets()).filter(t => t.type === 'page' && t.url === pinUrl).length;
const pinState = `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state')`;

if (phase === 1) {
  for (const url of [todayUrl, pinUrl]) {
    await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${url}', false), true`);
  }
  await until(() => evaluate(sidebar, `${APP}.tabs_.some(t => t.title === 'Icon restart-pin')`));
  await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.title === 'Icon restart-pin').id, '', 0, false), true`);
  const live = await until(() => evaluate(sidebar, `${pinState} === 'live'`));
  const today = await until(() => evaluate(sidebar, `${APP}.view_.today.some(t => t.title === 'Icon restart-today')`));
  if (!live || !today) {
    check('setup: a Today tab and a live pinned tab', false, JSON.stringify({live, today}));
    process.exit(1);
  }
  const version = await (await fetch(`${base}/json/version`)).json();
  const ws = new WebSocket(version.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Browser.close'}));
  for (let i = 0; i < 60 && alive(); i++) {
    await sleep(500);
  }
  process.exit(alive() ? 1 : 75);
}

const started = Date.now();
const seenAtStart = await pinPages();
// Without the session back, an asleep pinned tab would prove nothing.
check('after a restart the session comes back: the Today tab is there',
    !!(await until(() => evaluate(sidebar, `${APP}.view_.today.some(t => t.title === 'Icon restart-today')`))),
    JSON.stringify((await targets()).map(t => t.url)));
// The first sweep closes it, 10 s after the start (lifecycle_service.cc).
const closed = await until(async () => (await pinPages()) === 0, 20000);
console.log(`note: the pinned tab's restored page ${seenAtStart ? 'was open at the start' : 'was not open at the start'}` +
    `${closed ? `, closed ${Date.now() - started} ms in` : ''}`);
check('the tab the session restores for a pinned tab is closed', !!closed,
    JSON.stringify((await targets()).map(t => t.url)));
check('…and the pinned tab is in the sidebar, asleep',
    !!(await until(() => evaluate(sidebar, `${pinState} === 'asleep'`))), String(await evaluate(sidebar, pinState)));
await evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned').click(), true`);
check('clicked, it wakes', !!(await until(() => evaluate(sidebar, `${pinState} === 'live'`))),
    String(await evaluate(sidebar, pinState)));
await until(async () => (await pinPages()) > 0);
await sleep(1500);
check('…into exactly one tab', (await pinPages()) === 1, String(await pinPages()));
process.exit(results.every(Boolean) ? 0 : 1);
