// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Pins Today tabs by dragging them with real input events (CDP drag
// interception), as in Zen: onto the space header (top) and onto the Today
// header, the separator (end).
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();

for (const t of ['Alpha', 'Beta']) {
  await fetch(`${base}/json/new?${encodeURIComponent(`data:text/html,<title>${t}</title>${t}`)}`, {method: 'PUT'});
}
await sleep(1500);
const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const ws = new WebSocket(sidebar.webSocketDebuggerUrl);
await new Promise(r => (ws.onopen = r));
let nextId = 1;
const pending = new Map();
let intercepted = null;
ws.onmessage = e => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Input.dragIntercepted') intercepted = m.params.data;
};
const send = (method, params = {}) => new Promise(r => {
  const id = nextId++;
  pending.set(id, r);
  ws.send(JSON.stringify({id, method, params}));
});
const evaluate = async expr => (await send('Runtime.evaluate',
    {expression: expr, returnByValue: true})).result?.result?.value;
const root = `document.querySelector('cromarc-sidebar-app').shadowRoot`;
const center = async selector => evaluate(`(() => {
  const r = ${root}.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();
  return {x: r.x + r.width / 2, y: r.y + r.height / 2};
})()`);
const todayRow = title => `[...${root}.querySelectorAll('.row.today')].find(r => r.textContent.includes('${title}'))`;
const pinned = () => evaluate(`[...${root}.querySelectorAll('#tree .row.pinned .title')].map(e => e.textContent)`);

async function drag(title, targetSelector, overClassCheck) {
  const from = await evaluate(`(() => { const r = ${todayRow(title)}.getBoundingClientRect(); return {x: r.x + 40, y: r.y + r.height / 2}; })()`);
  const to = await center(targetSelector);
  intercepted = null;
  await send('Input.setInterceptDrags', {enabled: true});
  await send('Input.dispatchMouseEvent', {type: 'mousePressed', x: from.x, y: from.y, button: 'left', buttons: 1, clickCount: 1});
  for (let i = 1; i <= 5 && !intercepted; i++) {
    await send('Input.dispatchMouseEvent', {type: 'mouseMoved', x: from.x, y: from.y - i * 4, button: 'left', buttons: 1});
    await sleep(50);
  }
  if (!intercepted) return {started: false};
  await send('Input.dispatchDragEvent', {type: 'dragEnter', x: to.x, y: to.y, data: intercepted});
  await send('Input.dispatchDragEvent', {type: 'dragOver', x: to.x, y: to.y, data: intercepted});
  await sleep(100);
  const highlighted = await evaluate(overClassCheck);
  await send('Input.dispatchDragEvent', {type: 'drop', x: to.x, y: to.y, data: intercepted});
  await send('Input.dispatchMouseEvent', {type: 'mouseReleased', x: to.x, y: to.y, button: 'left', buttons: 0, clickCount: 1});
  await sleep(800);
  return {started: true, highlighted};
}

check('setup: nothing pinned', (await pinned()).length === 0, JSON.stringify(await pinned()));
let r = await drag('Beta', '.space-header', `${root}.querySelector('.space-header').classList.contains('over')`);
check('a Today tab drag starts', r.started);
check('the space header lights up under the drag', r.highlighted === true);
check('dropped on the space header: pinned', JSON.stringify(await pinned()) === '["Beta"]', JSON.stringify(await pinned()));
r = await drag('Alpha', '#sidebar .separator', `${root}.querySelector('#tree-end').classList.contains('over')`);
check('over the separator the landing line shows', r.highlighted === true);
check('dropped on the separator: pinned after the last one', JSON.stringify(await pinned()) === '["Beta","Alpha"]', JSON.stringify(await pinned()));
check('the highlight is gone after the drop', await evaluate(`!${root}.querySelector('.over')`));
ws.close();
console.log(`${results.filter(Boolean).length}/${results.length}`);
process.exit(results.every(Boolean) ? 0 : 1);
