// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Library (archive) and Zen's edge swipes.
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
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
const sb = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const LIB = `${APP}.shadowRoot.querySelector('cromarc-library')`;

// Two Today tabs, then Clear: they go to the archive.
for (const p of ['a.html', 'b.html']) {
  await send(await browserWs(), 'Target.createTarget', {url: `${site}/${p}`});
}
await until(async () => (await evaluate(sb, `${APP}.tabs_.length`)) >= 3);
await sleep(1500);
await evaluate(sb, `${APP}.proxy_.handler.clearToday()`);
await sleep(800);

await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
const rows = await until(async () => {
  const r = await evaluate(sb, `${LIB}?.shadowRoot.querySelectorAll('.row').length`);
  return r >= 2 && r;
});
check('🗃 opens the Library with the archived tabs', rows >= 2, String(rows));
const firstGroup = await evaluate(sb, `${LIB}.shadowRoot.querySelector('.day').textContent`);
const sub = await evaluate(sb, `${LIB}.shadowRoot.querySelector('.row .sub').textContent`);
console.log('names:', JSON.stringify(await evaluate(sb, `[...${LIB}.shadowRoot.querySelectorAll('.row .name')].map(n => n.textContent + ' | ' + n.closest('.row').title)`)));
check('grouped by day, with the reason and time', firstGroup === 'Сегодня' && sub.includes('«Очистить»'), `${firstGroup} | ${sub}`);
await evaluate(sb, `(() => { const s = ${LIB}.shadowRoot.querySelector('#search'); s.value = 'little'; s.dispatchEvent(new Event('input')); })()`);
const filtered = await until(async () => {
  const names = await evaluate(sb, `[...${LIB}.shadowRoot.querySelectorAll('.row .name')].map(n => n.textContent)`);
  return names.length === 1 && names;
});
check('search filters the archive', filtered?.[0] === 'Little test', JSON.stringify(await evaluate(sb, `[...${LIB}.shadowRoot.querySelectorAll('.row .name')].map(n => n.textContent)`)) + ' q=' + await evaluate(sb, `${LIB}.query_`));
await evaluate(sb, `${LIB}.shadowRoot.querySelector('.row').click()`);
const restored = await until(async () => (await evaluate(sb, `${APP}.tabs_.map(t => t.url)`)).some(u => u.endsWith('a.html')));
check('a click brings it back to Today', !!restored);
const leftRows = await evaluate(sb, `${LIB}.shadowRoot.querySelectorAll('.row').length`);
check('…and it leaves the archive', leftRows === 0, String(leftRows));
await evaluate(sb, `${LIB}.shadowRoot.querySelector('#close').click()`);
check('✕ closes the Library', (await until(async () => !(await evaluate(sb, `!!${LIB}`)))) === true);

// Edge swipes (the only space is both the first and the last).
const swipe = async (amount, direction) => evaluate(sb, `(async () => {
  const app = ${APP};
  app.onSwipe_({type: 0, amount: 0, direction: 0});
  for (let i = 1; i <= 10; i++) app.onSwipe_({type: 1, amount: ${amount} * i / 10, direction: 0});
  await app.updateComplete;
  const state = {drag: app.libraryDrag_, add: app.addProgress_};
  app.onSwipe_({type: 2, amount: ${amount}, direction: ${direction}});
  await app.updateComplete;
  return state;
})()`);
const lib = await swipe(-0.6, -1);
check('swiping back past the first space pulls the Library in', lib.drag > 0, JSON.stringify(lib));
check('…and it opens', (await until(() => evaluate(sb, `${APP}.libraryOpen_ && !!${LIB}`))) === true);
await evaluate(sb, `${APP}.onLibraryClose_()`);
await sleep(400);
const add = await swipe(0.9, 1);
check('swiping on past the last space fills the + arc', add.add >= 1, JSON.stringify(add));
check('…and opens the new space form', (await until(() => evaluate(sb, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`))) === true);
await evaluate(sb, `${APP}.onSpaceEditCancel_()`);
const short = await swipe(0.3, 1);
check('a short swipe does not', short.add < 1 && !(await evaluate(sb, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`)), JSON.stringify(short));
process.exit(results.every(Boolean) ? 0 : 1);
