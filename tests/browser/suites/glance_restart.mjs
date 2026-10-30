// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A glance is a temporary page, as a Little window is: a restart does not
// bring it back as a tab, and it is in the archive.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const phase = Number(process.env.CROMARC_TEST_PHASE ?? 1);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => {
    const timer = setTimeout(() => r({timeout: true}), 5000);
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) { clearTimeout(timer); r(m); } };
    ws.onclose = () => { clearTimeout(timer); r({closed: true}); };
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
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);
const mouse = (t, type, x, y, modifiers = 0) => send(t, 'Input.dispatchMouseEvent',
    {type, x, y, modifiers, button: type === 'mouseMoved' ? 'none' : 'left', clickCount: type === 'mouseMoved' ? 0 : 1});

const APP = `document.querySelector('cromarc-sidebar-app')`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const source = `${site}/glance.html`;
const preview = `${site}/b.html`;

if (phase === 1) {
  const tab = (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
  await send(tab, 'Page.navigate', {url: source});
  const page = await until(() => pageAt(source));
  await sleep(800);
  await mouse(page, 'mouseMoved', 120, 80);
  await sleep(400);
  await mouse(page, 'mousePressed', 120, 80, 1);
  await mouse(page, 'mouseReleased', 120, 80, 1);
  const glance = await until(async () => (await pageAt(preview)) &&
      (await evaluate(sidebar, `(${APP}.glances_ ?? []).length`)) === 1);
  check('setup: a glance is open at quit', !!glance);
  // Lets the session save the window with both tabs.
  await sleep(2500);
  await send(await browserWs(), 'Browser.close');
  process.exit(results.every(Boolean) ? 75 : 1);
}

await until(() => pageAt(source), 15000);
await sleep(3000);
check('after a restart its tab is back', !!(await pageAt(source)));
check('…but the glance is not back as a tab', !(await pageAt(preview)),
    JSON.stringify((await targets()).filter(t => t.type === 'page').map(t => t.url)));
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('', 50).then(({tabs}) => tabs.map(t => [t.url, t.reason]))`);
check('…it is in the archive',
    !!(await until(async () => (await archived()).some(([url, reason]) => url === preview && reason === 'glance'))),
    JSON.stringify(await archived()));
process.exit(results.every(Boolean) ? 0 : 1);
