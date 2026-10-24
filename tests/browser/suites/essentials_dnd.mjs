// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Essentials by drag and drop, as Zen: the promo in empty Essentials, the
// slot that opens in the grid, the drag image as a tile, reordering, an
// essential dropped on the tree becoming a pinned tab. Real drag events (CDP
// drag interception).
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();

for (const t of ['Alpha', 'Beta', 'Gamma']) {
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
const evaluate = async expr => {
  const r = await send('Runtime.evaluate', {expression: expr, returnByValue: true, awaitPromise: true});
  if (r.result?.exceptionDetails) console.log('EXC', JSON.stringify(r.result.exceptionDetails).slice(0, 300));
  return r.result?.result?.value;
};
const root = `document.querySelector('cromarc-sidebar-app').shadowRoot`;
const app = `document.querySelector('cromarc-sidebar-app')`;
const rect = async expr => evaluate(`(() => { const r = (${expr}).getBoundingClientRect(); return {x: r.x, y: r.y, w: r.width, h: r.height}; })()`);
const centerOf = r => ({x: r.x + r.w / 2, y: r.y + r.h / 2});
const todayRow = title => `[...${root}.querySelectorAll('.row.today')].find(r => r.textContent.includes('${title}'))`;
const treeRow = title => `[...${root}.querySelectorAll('#tree .row.pinned')].find(r => r.textContent.includes('${title}'))`;
const essentialTile = title => `${root}.querySelector('#essentials .essential[title="${title}"]')`;
const essentials = () => evaluate(`[...${root}.querySelectorAll('#essentials .essential[data-id]')].map(e => e.title)`);
const pinned = () => evaluate(`[...${root}.querySelectorAll('#tree .row.pinned .title')].map(e => e.textContent)`);
const ghost = () => evaluate(`(() => { const g = ${root}.querySelector('#drag-ghost').firstElementChild; if (!g) return null; const r = g.getBoundingClientRect(); return {cls: g.className, w: Math.round(r.width), h: Math.round(r.height)}; })()`);
const slotIndex = () => evaluate(`[...${root}.querySelectorAll('#essentials .essential:not(.dragging)')].findIndex(e => e.classList.contains('slot'))`);

async function start(fromExpr) {
  const r = await rect(fromExpr);
  const from = {x: r.x + Math.min(40, r.w / 2), y: r.y + r.h / 2};
  intercepted = null;
  await send('Input.setInterceptDrags', {enabled: true});
  await send('Input.dispatchMouseEvent', {type: 'mousePressed', x: from.x, y: from.y, button: 'left', buttons: 1, clickCount: 1});
  for (let i = 1; i <= 5 && !intercepted; i++) {
    await send('Input.dispatchMouseEvent', {type: 'mouseMoved', x: from.x, y: from.y - i * 4, button: 'left', buttons: 1});
    await sleep(50);
  }
  await sleep(50);
  return !!intercepted;
}
let entered = false;
async function over(p) {
  if (!entered) {
    await send('Input.dispatchDragEvent', {type: 'dragEnter', x: p.x, y: p.y, data: intercepted});
    entered = true;
  }
  // Blink fires only dragenter/dragleave when the element under the cursor
  // changes, and dragover on the next move: a real drag moves continuously.
  await send('Input.dispatchDragEvent', {type: 'dragOver', x: p.x, y: p.y, data: intercepted});
  await sleep(30);
  await send('Input.dispatchDragEvent', {type: 'dragOver', x: p.x, y: p.y, data: intercepted});
  await sleep(120);
}
async function drop(p) {
  await send('Input.dispatchDragEvent', {type: 'drop', x: p.x, y: p.y, data: intercepted});
  await send('Input.dispatchMouseEvent', {type: 'mouseReleased', x: p.x, y: p.y, button: 'left', buttons: 0, clickCount: 1});
  entered = false;
  await sleep(800);
}
const zone = async () => centerOf(await rect(`${root}.querySelector('#essentials')`));

check('setup: no essentials', (await essentials()).length === 0, JSON.stringify(await essentials()));
check('empty Essentials leave a drop zone (≥ 16px)', (await rect(`${root}.querySelector('#essentials')`)).h >= 16);

// 1. Empty Essentials: the promo.
check('a Today tab drag starts', await start(todayRow('Alpha')));
check('the drag image is the row, not the native one', (await ghost())?.cls.includes('row'), JSON.stringify(await ghost()));
await over(await zone());
check('over empty Essentials the promo shows', await evaluate(`!!${root}.querySelector('.essentials-promo')`));
check('the promo has Zen\'s text', await evaluate(`${root}.querySelector('.essentials-promo').textContent.replace(/\\s+/g, ' ').trim()`) === 'Добавить в важное Доступ к любимым вкладкам в один клик',
    await evaluate(`${root}.querySelector('.essentials-promo')?.textContent`));
const promo = centerOf(await rect(`${root}.querySelector('.essentials-promo')`));
await over(promo);
check('over the promo it turns solid', await evaluate(`${root}.querySelector('.essentials-promo').classList.contains('over')`));
check('the drag image turns into a tile', (await ghost())?.cls.includes('essential'), JSON.stringify(await ghost()));
const below = centerOf(await rect(`${root}.querySelector('.space-header')`));
await over({x: below.x, y: below.y + 120});
check('away from Essentials the promo stays, dashed', await evaluate(`(() => { const p = ${root}.querySelector('.essentials-promo'); return !!p && !p.classList.contains('over'); })()`));
check('away from Essentials the drag image is a row again', (await ghost())?.cls.includes('row'), JSON.stringify(await ghost()));
await over(centerOf(await rect(`${root}.querySelector('.essentials-promo')`)));
await drop(centerOf(await rect(`${root}.querySelector('.essentials-promo')`)));
check('dropped on the promo: an essential', JSON.stringify(await essentials()) === '["Alpha"]', JSON.stringify(await essentials()));
check('it left Today', !(await evaluate(`!!${todayRow('Alpha')}`)));
check('no promo, slot or drag image after the drop', await evaluate(`!${root}.querySelector('.essentials-promo, .essential.slot, #drag-ghost > *')`));
const grid = await rect(`${root}.querySelector('#essentials')`);
const one = await rect(essentialTile('Alpha'));
check('one essential spans the grid (Zen auto-fit)', Math.abs(one.w - grid.w) < 2, `${one.w} vs ${grid.w}`);
check('tile height 46px', Math.round(one.h) === 46, `${one.h}`);

// 2. Non-empty grid: the slot opens at the end, then follows the drag.
check('drag Beta', await start(todayRow('Beta')));
await over(centerOf(await rect(essentialTile('Alpha'))));
check('over the grid an empty slot opens at the end', await slotIndex() === 1, `${await slotIndex()}`);
const slotW = Math.round((await rect(`${root}.querySelector('.essential.slot')`)).w);
await over(centerOf(await rect(essentialTile('Alpha'))));
const g = await ghost();
check('the tile drag image is as wide as the slot', g?.cls.includes('essential') && Math.abs(g.w - slotW) <= 1, `${JSON.stringify(g)} slot ${slotW}`);
const alpha = await rect(essentialTile('Alpha'));
await send('Input.dispatchDragEvent', {type: 'dragOver', x: alpha.x + 2, y: alpha.y + 2, data: intercepted});
await sleep(30);
await send('Input.dispatchDragEvent', {type: 'dragOver', x: alpha.x + 2, y: alpha.y + 2, data: intercepted});
check('the tile steps aside with an animation', await evaluate(`${essentialTile('Alpha')}.getAnimations().length > 0`));
await sleep(120);
check('over the first tile the slot moves there', await slotIndex() === 0, `${await slotIndex()}`);
await drop({x: alpha.x + 2, y: alpha.y + 2});
check('dropped: Beta before Alpha', JSON.stringify(await essentials()) === '["Beta","Alpha"]', JSON.stringify(await essentials()));
check('two essentials: Zen layout 2', await evaluate(`${root}.querySelector('#essentials').getAttribute('layout')`) === '2');

// 3. A slot shown once stays until the drag ends; elsewhere the drop pins.
check('drag Gamma', await start(todayRow('Gamma')));
await over(await zone());
await over(await zone());
check('slot open', await slotIndex() >= 0);
const header = centerOf(await rect(`${root}.querySelector('.space-header')`));
await over(header);
check('over the space header the slot stays (Zen keeps its fake tile)', await slotIndex() >= 0);
await drop(header);
check('dropped on the header: pinned, not essential', JSON.stringify(await pinned()) === '["Gamma"]' && (await essentials()).length === 2,
    `${JSON.stringify(await pinned())} ${JSON.stringify(await essentials())}`);
check('no slot after the drop', await slotIndex() === -1);

// 4. Reordering essentials: the dragged one is hidden, its place open.
check('drag the essential Alpha', await start(essentialTile('Alpha')));
await over(centerOf(await rect(`${root}.querySelector('.essential.slot')`)));
check('the dragged essential is hidden', await evaluate(`${essentialTile('Alpha')}.classList.contains('dragging')`));
check('its place stays open', await slotIndex() === 1, `${await slotIndex()}`);
check('its drag image is the tile', (await ghost())?.cls.includes('essential'), JSON.stringify(await ghost()));
const beta = await rect(essentialTile('Beta'));
await over({x: beta.x + beta.w / 2, y: beta.y + beta.h / 2});
await over({x: beta.x + beta.w / 2 - 1, y: beta.y + beta.h / 2});
check('over Beta the slot moves before it', await slotIndex() === 0, `${await slotIndex()}`);
await drop({x: beta.x + beta.w / 2 - 1, y: beta.y + beta.h / 2});
if (JSON.stringify(await essentials()) !== '["Alpha","Beta"]') console.log('tree', JSON.stringify(await evaluate(`${app}.tree_.map(n => [n.title, n.essential])`)));
check('reordered: Alpha before Beta', JSON.stringify(await essentials()) === '["Alpha","Beta"]', JSON.stringify(await essentials()));

// 5. An essential dropped on a pinned row becomes a pinned tab there.
check('drag the essential Beta', await start(essentialTile('Beta')));
const gamma = await rect(treeRow('Gamma'));
await over({x: gamma.x + 40, y: gamma.y + 4});
check('outside Essentials an essential drags as a row', (await ghost())?.cls.includes('row'), JSON.stringify(await ghost()));
await drop({x: gamma.x + 40, y: gamma.y + 4});
check('dropped on a row: pinned before it, not essential',
    JSON.stringify(await pinned()) === '["Beta","Gamma"]' && JSON.stringify(await essentials()) === '["Alpha"]',
    `${JSON.stringify(await pinned())} ${JSON.stringify(await essentials())}`);

// 6. The menu adds to the end of the grid (Zen appends).
await evaluate(`${app}.testMenuChoice = 'Добавить в важное'`);
await evaluate(`${treeRow('Beta')}.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true, clientX: 50, clientY: 50}))`);
await sleep(800);
check('the menu adds at the end', JSON.stringify(await essentials()) === '["Alpha","Beta"]', JSON.stringify(await essentials()));

// 7. Folders don't go into Essentials.
await evaluate(`${app}.testMenuChoice = 'Новая папка'`);
await evaluate(`${root}.querySelector('#sidebar').dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true, clientX: 50, clientY: 400}))`);
await sleep(800);
await evaluate(`${root}.querySelector('input.rename')?.blur()`);
await sleep(500);
const folder = `${root}.querySelector('#tree .row.folder')`;
if (await evaluate(`!!${folder}`)) {
  check('drag a folder', await start(folder));
  await over(centerOf(await rect(essentialTile('Alpha'))));
  await over(centerOf(await rect(essentialTile('Alpha'))));
  check('a folder opens no slot', await slotIndex() === -1);
  await drop(centerOf(await rect(essentialTile('Alpha'))));
  check('and stays a folder', JSON.stringify(await essentials()) === '["Alpha","Beta"]', JSON.stringify(await essentials()));
} else {
  check('folder created', false);
}

ws.close();
console.log(`${results.filter(Boolean).length}/${results.length}`);
process.exit(results.every(Boolean) ? 0 : 1);
