// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A sleeping pinned tab, clicked, wakes in its place, as Zen's unloaded pinned
// tab loads where it is: its new tab never shows in Today on the way.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
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
const APP = `document.querySelector('cromarc-sidebar-app')`;
const ROOT = `${APP}.shadowRoot`;
const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const todayTitles = () => evaluate(sidebar,
    `[...${ROOT}.querySelectorAll('#today .row .title')].map(e => e.textContent.trim())`);

await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${site}/icon.html?wake', false), true`);
await until(() => evaluate(sidebar, `${APP}.tabs_.some(t => t.title === 'Icon wake')`));
await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.title === 'Icon wake').id, '', 0, false), true`);
await until(() => evaluate(sidebar, `${APP}.view_.rows.length === 1`));
await evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned button.close').click(), true`);
check('setup: the pinned tab asleep', !!(await until(() => evaluate(sidebar,
    `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state') === 'asleep'`))));
const before = await todayTitles();

// Every frame while it wakes: what Today shows.
await evaluate(sidebar, `(() => {
  window.todayFrames = [];
  const sample = () => {
    window.todayFrames.push([...${ROOT}.querySelectorAll('#today .row')].length);
    if (window.todayFrames.length < 120) requestAnimationFrame(sample);
  };
  requestAnimationFrame(sample);
  ${ROOT}.querySelector('#tree .row.pinned').click();
  return true;
})()`);
check('clicked, the pinned tab wakes', !!(await until(() => evaluate(sidebar,
    `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state') === 'live'`))));
await until(() => evaluate(sidebar, `window.todayFrames.length >= 120`), 5000);
const frames = await evaluate(sidebar, `window.todayFrames`);
check('…in its place: Today never showed its new tab',
    frames.every(n => n === before.length), JSON.stringify({before: before.length, frames}));
check('…and Today is as it was', JSON.stringify(await todayTitles()) === JSON.stringify(before),
    JSON.stringify(await todayTitles()));

// Pinned from Today (a drop on the pinned list): one step, never both places
// or neither, nor asleep on the way.
await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${site}/icon.html?pin', false), true`);
await until(() => evaluate(sidebar, `${APP}.view_.today.some(t => t.title === 'Icon pin')`));
const shape = `[[...${ROOT}.querySelectorAll('#today .row .title')].some(e => e.textContent.trim() === 'Icon pin'),
    [...${ROOT}.querySelectorAll('#tree .row.pinned')].filter(r => r.textContent.includes('Icon pin')).map(r => r.getAttribute('state')).join()]`;
await evaluate(sidebar, `(() => {
  window.pinFrames = [];
  const sample = () => {
    window.pinFrames.push(JSON.stringify(${shape}));
    if (window.pinFrames.length < 90) requestAnimationFrame(sample);
  };
  requestAnimationFrame(sample);
  ${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.title === 'Icon pin').id, '', 1, false);
  return true;
})()`);
await until(() => evaluate(sidebar, `window.pinFrames.length >= 90`), 5000);
const pinFrames = [...new Set(await evaluate(sidebar, `window.pinFrames`))];
check('a tab pinned from Today moves in one step',
    pinFrames.every(f => f === '[true,""]' || f === '[false,"live"]') &&
        pinFrames.includes('[false,"live"]'),
    JSON.stringify(pinFrames));
process.exit(results.every(Boolean) ? 0 : 1);
