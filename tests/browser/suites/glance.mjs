// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Glance: Alt+click on a link previews it; Esc closes; the page stays.
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
  // A target closed by this very call (Esc closing Glance) answers by closing.
  const msg = await new Promise(r => {
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); };
    ws.onclose = () => r({closed: true});
  });
  ws.close();
  return msg;
}
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const find = async url => (await targets()).find(t => t.url === url);
const mouse = (t, type, x, y, modifiers = 0) => send(t, 'Input.dispatchMouseEvent',
    {type, x, y, modifiers, button: type === 'mouseMoved' ? 'none' : 'left', clickCount: type === 'mouseMoved' ? 0 : 1});

let tab = (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
await send(tab, 'Page.navigate', {url: `${site}/glance.html`});
tab = await until(() => find(`${site}/glance.html`));
await sleep(800);

// Alt+click where there is no link: nothing.
await mouse(tab, 'mouseMoved', 100, 300);
await sleep(300);
await mouse(tab, 'mousePressed', 100, 300, 1);
await mouse(tab, 'mouseReleased', 100, 300, 1);
await sleep(800);
check('Alt+click off a link opens nothing', !(await find(`${site}/b.html`)));

await mouse(tab, 'mouseMoved', 120, 80);
await sleep(400);
await mouse(tab, 'mousePressed', 120, 80, 1);
await mouse(tab, 'mouseReleased', 120, 80, 1);
const glance = await until(() => find(`${site}/b.html`));
check('Alt+click on a link opens it in Glance', !!glance);
check('the page itself stays', !!(await find(`${site}/glance.html`)));
const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const stripTabs = await send(sidebar, 'Runtime.evaluate', {returnByValue: true,
  expression: `document.querySelector('cromarc-sidebar-app').tabs_.map(t => t.url)`});
check('…and no tab opens for it', JSON.stringify(stripTabs.result?.result?.value) === JSON.stringify([`${site}/glance.html`]),
    JSON.stringify(stripTabs.result));
const tabs = (await targets()).filter(t => t.type === 'page' && t.url.startsWith(site));
console.log('pages:', tabs.map(t => t.url));
const r = await send(glance, 'Runtime.evaluate', {expression: 'document.title', returnByValue: true});
check('the preview has loaded', r.result?.result?.value === 'Second link', JSON.stringify(r.result));

await sleep(600);
await send(glance, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 53});
check('Esc closes it', (await until(async () => !(await find(`${site}/b.html`)))) === true);

// Ordinary clicks are untouched: the link is followed.
await mouse(tab, 'mouseMoved', 120, 80);
await mouse(tab, 'mousePressed', 120, 80, 0);
await mouse(tab, 'mouseReleased', 120, 80, 0);
check('a plain click still follows the link', !!(await until(() => find(`${site}/b.html`))));
process.exit(results.every(Boolean) ? 0 : 1);
