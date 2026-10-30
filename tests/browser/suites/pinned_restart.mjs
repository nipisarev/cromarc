// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A window that holds only pinned tabs keeps one of them after a restart:
// the startup pass closes the pages the session restores for pinned tabs,
// and a page with a beforeunload listener leaves the strip only later, so
// the count is taken before closing (the window closed with all three,
// 29.10.2026). The runner restarts the browser (tooling/test-browser).
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
// Empty once the browser is gone.
const targets = async () => {
  try {
    return await (await fetch(`${base}/json/list`)).json();
  } catch {
    return [];
  }
};
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
const urls = ['a', 'b', 'c'].map(n => `${site}/unload.html?pin-${n}`);
const sidebar = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
// The window's tabs: every page but the sidebar and Cromarc's other panels.
const pages = async () => (await targets()).filter(t => t.type === 'page' && !t.url.startsWith('chrome://cromarc'));
const pinnedRows = async () => evaluate(await sidebar(), `${ROOT}.querySelectorAll('#tree .row.pinned').length`);

if (phase === 1) {
  const sb = await until(sidebar);
  for (const url of urls) {
    await evaluate(sb, `${APP}.proxy_.handler.openUrl('${url}', false), true`);
  }
  await until(() => evaluate(sb, `${JSON.stringify(urls)}.every(u => ${APP}.tabs_.some(t => t.url === u))`));
  for (const url of urls) {
    await evaluate(sb, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.url === '${url}').id, '', 0, false), true`);
    await until(async () => (await pinnedRows()) === urls.indexOf(url) + 1);
  }
  for (const page of await pages()) {
    if (!urls.includes(page.url)) {
      await fetch(`${base}/json/close/${page.id}`);
    }
  }
  const only = await until(async () => {
    const open = (await pages()).map(p => p.url).sort();
    return JSON.stringify(open) === JSON.stringify(urls) && open;
  });
  const live = await until(() => evaluate(sb,
      `[...${ROOT}.querySelectorAll('#tree .row.pinned')].every(r => r.getAttribute('state') === 'live')`));
  if (!only || !live || (await pinnedRows()) !== 3) {
    check('setup: the window holds only three live pinned tabs', false,
        JSON.stringify({pages: (await pages()).map(p => p.url), rows: await pinnedRows(), live}));
    process.exit(1);
  }
  // The session is written on a timer; the quit writes what is left.
  await sleep(1000);
  const version = await (await fetch(`${base}/json/version`)).json();
  const ws = new WebSocket(version.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Browser.close'}));
  for (let i = 0; i < 60 && alive(); i++) {
    await sleep(500);
  }
  process.exit(alive() ? 1 : 75);
}

const restored = await until(async () => (await pages()).filter(p => urls.includes(p.url)).length === 3, 8000);
check('after a restart the session brings back the three pinned pages', !!restored,
    JSON.stringify((await pages()).map(p => p.url)));
// What makes the pages close later: their listeners are loaded.
const loaded = [];
for (const page of (await pages()).filter(p => urls.includes(p.url))) {
  try {
    if (await evaluate(page, 'document.readyState') === 'complete') loaded.push(page.url);
  } catch {}
}
console.log(`note: ${loaded.length} of the pinned pages were loaded at the start`);
// The first sweep is 10 s after the start (lifecycle_service.cc); the pages
// it closes go once their listeners ran.
await until(async () => (await pages()).filter(p => urls.includes(p.url)).length < 3, 20000);
await sleep(3000);
check('the window is still there', !!(await sidebar()) && alive(),
    JSON.stringify({alive: alive(), targets: (await targets()).map(t => t.url)}));
const left = await pages();
check('…holding exactly one tab', left.length === 1, JSON.stringify(left.map(p => p.url)));
check('…and the three pinned tabs are in the sidebar',
    !!(await until(async () => (await pinnedRows()) === 3, 3000)), String(await pinnedRows().catch(() => 'no sidebar')));
process.exit(results.every(Boolean) ? 0 : 1);
