// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {texts} from '../strings.mjs';

// The reset button's sublabel stays on one line inside its row.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
async function cdp(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => r(JSON.parse(e.data))));
  ws.close();
  return msg.result;
}
const evaluate = async (t, expr) => (await cdp(t, 'Runtime.evaluate',
    {expression: `(async () => { ${expr} })()`, returnByValue: true, awaitPromise: true})).result?.value;
const page = t => `data:text/html,<title>${t}</title>${t}`;

await fetch(`${base}/json/new?${encodeURIComponent(page('Home Page'))}`, {method: 'PUT'});
await sleep(1500);
const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const prelude = `const app = document.querySelector('cromarc-sidebar-app');
  const root = app.shadowRoot;
  const row = (sel, title) => [...root.querySelectorAll(sel)].find(r => r.querySelector('.title')?.textContent.includes(title));
  const openMenu = async (el, x, y) => {
    el.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true, clientX: x, clientY: y}));
    await new Promise(r => setTimeout(r, 300));
    const m = root.querySelector('#menu').getBoundingClientRect();
    return {left: m.left, right: m.right, top: m.top, bottom: m.bottom, w: innerWidth, h: innerHeight};
  };
  const close = async () => { root.querySelector('#menu-backdrop').click(); await new Promise(r => setTimeout(r, 200)); };`;

let r;
const {pin} = await texts((t, expr) => evaluate(t, `return ${expr};`), sidebar, ['pin']);
// Pin it, take it away, and look at the sublabel when it shows.
await evaluate(sidebar, `${prelude}
  const el = row('#today .row', 'Home Page');
  app.testMenuChoice = ${JSON.stringify(pin)};
  el.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true, clientX: 20, clientY: 20}));
  await new Promise(r => setTimeout(r, 300));`);
await sleep(1000);
const tab = (await targets()).find(t => t.type === 'page' && t.title === 'Home Page');
await cdp(tab, 'Page.navigate', {url: page('Somewhere%20else%20with%20a%20long%20title')});
await sleep(1500);
r = await evaluate(sidebar, `${prelude}
  const p = row('#tree .row', 'Home Page');
  const sub = p.querySelector('.sublabel');
  sub.style.maxHeight = '10px'; sub.style.opacity = '.6'; sub.style.lineHeight = '1.2';
  await new Promise(r => setTimeout(r, 300));
  const s = sub.getBoundingClientRect(), rr = p.getBoundingClientRect(), t = p.querySelector('.title').getBoundingClientRect();
  sub.removeAttribute('style');
  return {sub: s.height, subTop: s.top, subBottom: s.bottom, rowTop: rr.top, rowBottom: rr.bottom, titleTop: t.top,
          ws: getComputedStyle(sub).whiteSpace, text: sub.textContent};`);
check('sublabel is one line', r && r.ws === 'nowrap' && r.sub <= 13, JSON.stringify(r));
check('title and sublabel stay inside the row', r && r.titleTop >= r.rowTop && r.subBottom <= r.rowBottom, JSON.stringify(r));
console.log(`${results.filter(Boolean).length}/${results.length}`);
process.exit(results.every(Boolean) ? 0 : 1);
