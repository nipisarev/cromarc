// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Zen's single toolbar: a space window has no toolbar above the page; the
// sidebar's top row holds back, forward and reload and the next row the
// address field, which opens the command bar. A popup keeps Chromium's
// toolbar.
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
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  // A page this very call closes (Esc on a glance) answers by closing.
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
const sidebarOf = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const inSidebar = async (sidebar, body) => evaluate(sidebar, `(async () => {
  const app = document.querySelector('cromarc-sidebar-app');
  const root = app.shadowRoot;
  ${body}
})()`);
const click = (sidebar, selector) => inSidebar(sidebar, `root.querySelector(${JSON.stringify(selector)}).click(); return true;`);

const sidebar = await until(sidebarOf, 15000);
let page = (await targets()).find(t => t.type === 'page' && !t.url.startsWith('chrome://cromarc'));
await send(page, 'Page.navigate', {url: `${site}/a.html`});
page = await until(() => pageAt(`${site}/a.html`));
await send(page, 'Page.navigate', {url: `${site}/b.html`});
page = await until(() => pageAt(`${site}/b.html`));

// The page's window: with no toolbar the page runs to the top, Zen's 8px
// separation above and below it.
const {windowId} = (await send(await browserWs(), 'Browser.getWindowForTarget', {targetId: page.id})).result ?? {};
const {bounds} = (await send(await browserWs(), 'Browser.getWindowBounds', {windowId})).result ?? {};
const innerHeight = await evaluate(page, 'window.innerHeight');
check('no toolbar above the page: it takes the window height but the separation',
      bounds && bounds.height - innerHeight <= 20, `window ${bounds?.height}, page ${innerHeight}`);

const top = await until(() => inSidebar(sidebar, `
  const nav = app.navigation_;
  return nav.display === ${JSON.stringify(`${site}/b.html`)} && {
    row: !!root.querySelector('#zen-sidebar-top-buttons'),
    back: !root.querySelector('#back-button').disabled,
    forward: !root.querySelector('#forward-button').disabled,
    text: root.querySelector('.urlbar-input').textContent.trim(),
    lights: root.querySelector('.titlebar-buttonbox-container').getBoundingClientRect().width,
  };`));
check('the sidebar has Zen\'s top row', top?.row, JSON.stringify(top));
check('the address field shows the page (an http address in full)', top?.text === `${site}/b.html`, JSON.stringify(top));
check('back is on after a navigation, forward is off', top?.back && !top?.forward, JSON.stringify(top));
check('the top row leaves room for the window buttons', top?.lights > 0, JSON.stringify(top));
// Chromium's extensions container (its puzzle button at least) moves to the
// end of the address field, which leaves it room.
const room = await until(() => inSidebar(sidebar,
    `return root.querySelector('#page-action-buttons').getBoundingClientRect().width;`));
check('the address field leaves room for the extension buttons', room > 0, String(room));

await click(sidebar, '#back-button');
check('back goes back', !!(await until(() => pageAt(`${site}/a.html`))));
const forwardOn = await until(() => inSidebar(sidebar, `return !root.querySelector('#forward-button').disabled;`));
check('forward is on after going back', !!forwardOn);
await click(sidebar, '#forward-button');
page = await until(() => pageAt(`${site}/b.html`));
check('forward goes forward', !!page);

await evaluate(page, 'window.__mark = 1');
await click(sidebar, '#reload-button');
const reloaded = await until(async () => {
  const p = await pageAt(`${site}/b.html`);
  return p && (await evaluate(p, 'document.readyState === "complete" && !window.__mark'));
});
check('reload reloads the page', !!reloaded);

await click(sidebar, '#urlbar');
const bar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-command-bar')));
const barText = bar && await until(() => evaluate(bar, `(() => {
  const app = document.querySelector('cromarc-command-bar-app');
  const input = app?.shadowRoot?.querySelector('input');
  return input && input.value;
})()`));
check('the address field opens the command bar on the page\'s address', barText === `${site}/b.html`, String(barText));

// A popup keeps Chromium's toolbar: its page sits under a title and a
// location bar.
page = await pageAt(`${site}/b.html`);
// With a user gesture, as a click would give it: the popup blocker lets it be.
await send(page, 'Runtime.evaluate', {userGesture: true, expression:
    `window.open(${JSON.stringify(`${site}/c.html`)}, 'p', 'popup,width=500,height=400')`});
const popup = await until(() => pageAt(`${site}/c.html`));
const gap = popup && await evaluate(popup, 'window.outerHeight - window.innerHeight');
check('a popup keeps Chromium\'s toolbar', gap > 40, String(gap));

process.exit(results.every(Boolean) ? 0 : 1);
