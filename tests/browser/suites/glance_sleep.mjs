// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A glance lives and dies with its tab (Zen and Arc time out neither): when
// an unused Essential falls asleep, its glance goes to the archive. Run with
// --cromarc-idle-limit=8 (tooling/test-browser).
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const other = site.replace('127.0.0.1', 'localhost');
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
async function evaluate(target, expression, userGesture = false) {
  const m = await send(target, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true, userGesture});
  if (m.result?.exceptionDetails) throw new Error(JSON.stringify(m.result.exceptionDetails).slice(0, 300));
  return m.result?.result?.value;
}
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);
const loaded = async url => {
  const page = await until(() => pageAt(url));
  return page && await until(async () => (await evaluate(page, 'document.readyState')) === 'complete' && page);
};

const APP = `document.querySelector('cromarc-sidebar-app')`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('', 50).then(({tabs}) => tabs.map(t => [t.url, t.reason]))`);

const essentialUrl = `${site}/essentials.html?s`;
const start = (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
await send(start, 'Page.navigate', {url: essentialUrl});
const essential = await loaded(essentialUrl);
await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.url === '${essentialUrl}').id, '', 0, true), true`);
await sleep(500);
await evaluate(essential, `document.querySelector('#other').click(), true`, true);
const glanceUrl = `${other}/essentials.html?inner-s`;
check('setup: a glance over the Essential',
    !!(await until(async () => (await pageAt(glanceUrl)) && (await evaluate(sidebar, `(${APP}.glances_ ?? []).length`)) === 1)));

// While the glance is shown, the Essential is in use: it does not sleep.
await sleep(9000);
check('an Essential whose glance is shown does not fall asleep',
    !!(await pageAt(glanceUrl)) && !!(await pageAt(essentialUrl)));

// Away from it, both idle; the Essential sleeps, its glance goes to the archive.
await send(await browserWs(), 'Target.createTarget', {url: `${site}/a.html`});
check('away from it, the Essential falls asleep and its glance closes',
    !!(await until(async () => !(await pageAt(essentialUrl)) && !(await pageAt(glanceUrl)), 20000)),
    JSON.stringify((await targets()).filter(t => t.type === 'page').map(t => t.url)));
check('…the glance is in the archive, as a glance',
    !!(await until(async () => (await archived()).some(([url, reason]) => url === glanceUrl && reason === 'glance'))),
    JSON.stringify(await archived()));
process.exit(results.every(Boolean) ? 0 : 1);
