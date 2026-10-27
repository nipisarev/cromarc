// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The command bar: one field over the window that finds open tabs, pinned
// items and archived pages, driven from the keyboard, and opens what is
// chosen where it already is rather than a second time.
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
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
// A key as the keyboard sends it; with its Mac key code it also reaches the
// browser's own shortcuts when the page leaves it.
const KEYS = {
  ArrowDown: {code: 'ArrowDown', vk: 40, mac: 125},
  ArrowUp: {code: 'ArrowUp', vk: 38, mac: 126},
  Enter: {code: 'Enter', vk: 13, mac: 36},
  Escape: {code: 'Escape', vk: 27, mac: 53},
  l: {code: 'KeyL', vk: 76, mac: 37, text: 'l'},
};
async function press(target, key, modifiers = 0) {
  const k = KEYS[key];
  const params = {key, code: k.code, windowsVirtualKeyCode: k.vk, nativeVirtualKeyCode: k.mac, modifiers};
  const text = k.text ? {text: k.text, unmodifiedText: k.text} : {};
  await send(target, 'Input.dispatchKeyEvent', {type: k.text ? 'keyDown' : 'rawKeyDown', ...params, ...text});
  await send(target, 'Input.dispatchKeyEvent', {type: 'keyUp', ...params});
}

const APP = `document.querySelector('cromarc-sidebar-app')`;
const ROOT = `${APP}.shadowRoot`;
const BAR = `document.querySelector('cromarc-command-bar-app').shadowRoot`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const bar = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-command-bar'));
const pages = async () => (await targets()).filter(t => t.type === 'page' && t.url.startsWith(site));
const activeTitle = () => evaluate(sidebar, `${APP}.tabs_.find(t => t.isActive)?.title`);
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('', 50).then(({tabs}) => tabs.map(t => t.url))`);
const rows = async () => evaluate(await bar(), `[...${BAR}.querySelectorAll('.result')].map(r => ({
  title: r.querySelector('.title').textContent.trim(),
  url: r.querySelector('.subtitle').textContent.trim(),
  kind: r.querySelector('.kind').textContent.trim(),
  icon: r.querySelector('.icon').style.backgroundImage,
  selected: r.hasAttribute('selected'),
}))`);
const shown = async () => {
  const b = await bar();
  return !!b && await evaluate(b, `document.visibilityState === 'visible'`);
};
const openBar = async () => {
  await evaluate(sidebar, `${ROOT}.querySelector('.new-tab').click(), true`);
  return until(async () => (await shown()) && await evaluate(await bar(), `${BAR}.activeElement?.id === 'input'`));
};
// Typed over whatever the field holds, as typing over a selection does.
const type = async text => {
  const b = await bar();
  await evaluate(b, `(() => { const i = ${BAR}.querySelector('#input'); i.focus(); i.select(); return true; })()`);
  await send(b, 'Input.insertText', {text});
};
const rowsFor = (text, ready) => until(async () => {
  const b = await bar();
  if (await evaluate(b, `${BAR}.querySelector('#input').value`) !== text) return null;
  const r = await rows();
  return ready(r) ? r : null;
});
const TAB = 'Перейти к вкладке';
const PINNED = 'Закреплено';
const ARCHIVE = 'Из архива';
const url = q => `${site}/icon.html?${q}`;

// A sleeping pinned item, an archived page, and five tabs that match. Clear
// keeps a window's last tab, so the pinned tab is still open for it.
await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${url('bar-pin')}', false), true`);
await until(() => evaluate(sidebar, `${APP}.tabs_.some(t => t.title === 'Icon bar-pin')`));
await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.title === 'Icon bar-pin').id, '', 0, false), true`);
await until(() => evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state') === 'live'`));
await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${url('bar-old')}', false), true`);
await until(() => evaluate(sidebar, `${APP}.tabs_.some(t => t.title === 'Icon bar-old')`));
await evaluate(sidebar, `${APP}.proxy_.handler.clearToday(), true`);
await until(async () => (await archived()).includes(url('bar-old')));
for (const q of ['bar-1', 'bar-2', 'bar-3', 'bar-4', 'bar-5', 'other']) {
  await evaluate(sidebar, `${APP}.proxy_.handler.openUrl('${url(q)}', false), true`);
}
await until(async () => (await pages()).length === 7);
await evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned button.close').click(), true`);
const ready = await until(async () => (await pages()).length === 6 &&
    await evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state') === 'asleep'`) &&
    (await archived()).includes(url('bar-old')) &&
    await evaluate(sidebar, `${APP}.tabs_.filter(t => t.title.startsWith('Icon ')).length === 6`));
check('setup: six tabs, a pinned item asleep and a page in the archive', !!ready,
    JSON.stringify({pages: (await pages()).map(p => p.url), archived: await archived()}));

check('a new tab shows the command bar, its field empty', !!(await openBar()) &&
    await evaluate(await bar(), `${BAR}.querySelector('#input').value === ''`));

await type('ICON Bar');
const found = await rowsFor('ICON Bar', r => r.some(x => x.kind === ARCHIVE));
const tabRows = (found ?? []).filter(r => r.kind === TAB);
check('typed text lists at most 4 open tabs', tabRows.length === 4, JSON.stringify(found));
check('…each holding every typed word, case ignored', tabRows.length > 0 &&
    tabRows.every(r => /icon/i.test(r.title + r.url) && /bar/i.test(r.title + r.url)) &&
    !(found ?? []).some(r => r.url === url('other')), JSON.stringify(found));
check('the pinned item is listed, labelled «Закреплено»',
    (found ?? []).filter(r => r.url === url('bar-pin')).map(r => r.kind).join() === PINNED, JSON.stringify(found));
check('the archived page is listed, labelled «Из архива»',
    (found ?? []).filter(r => r.url === url('bar-old')).map(r => r.kind).join() === ARCHIVE, JSON.stringify(found));
check('each address is listed once', !!found && new Set(found.map(r => r.url)).size === found.length,
    JSON.stringify(found));
check('each tab, pinned item and archived page shows a favicon, a title and its address',
    !!found && found.filter(r => r.kind).every(r => r.icon.includes('favicon') && r.title && r.url.startsWith('http')),
    JSON.stringify(found));
check('…and tabs, pinned items and archived pages a label of their kind',
    !!found && found.filter(r => r.url.startsWith(url('bar'))).every(r => [TAB, PINNED, ARCHIVE].includes(r.kind)),
    JSON.stringify(found));

const selected = async () => (await rows()).findIndex(r => r.selected);
check('…the first row selected', (await selected()) === 0, String(await selected()));
await press(await bar(), 'ArrowDown');
check('Down moves the selection to the next row', !!(await until(async () => (await selected()) === 1)),
    String(await selected()));
await press(await bar(), 'ArrowUp');
check('Up moves it back', !!(await until(async () => (await selected()) === 0)), String(await selected()));

// Enter on the row Down selected.
const choose = async (text, pick) => {
  if (!(await openBar())) return false;
  await type(text);
  const r = await rowsFor(text, r => r.some(pick));
  if (!r) return false;
  const index = r.findIndex(pick);
  for (let i = 0; i < index; i++) {
    await press(await bar(), 'ArrowDown');
  }
  if (!(await until(async () => (await selected()) === index))) return false;
  await press(await bar(), 'Enter');
  return true;
};

let before = (await pages()).length;
check('choosing an open tab with Enter switches to it',
    await choose('bar-2', r => r.kind === TAB && r.url === url('bar-2')) &&
    !!(await until(async () => (await activeTitle()) === 'Icon bar-2')), String(await activeTitle()));
check('…the bar closes', !!(await until(async () => !(await shown()))));
check('…and no tab opens for it', (await pages()).length === before, JSON.stringify((await pages()).map(p => p.url)));

check('choosing the pinned item opens it in place',
    await choose('bar-pin', r => r.kind === PINNED) &&
    !!(await until(() => evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state') === 'live'`))),
    String(await evaluate(sidebar, `${ROOT}.querySelector('#tree .row.pinned')?.getAttribute('state')`)));
await sleep(500);
check('…as its one tab, never in Today',
    (await pages()).filter(p => p.url === url('bar-pin')).length === 1 &&
    !(await evaluate(sidebar, `${APP}.view_.today.some(t => t.title === 'Icon bar-pin')`)),
    JSON.stringify((await pages()).map(p => p.url)));

check('choosing an archived page reopens it',
    await choose('bar-old', r => r.kind === ARCHIVE) &&
    !!(await until(async () => (await pages()).some(p => p.url === url('bar-old')))),
    JSON.stringify((await pages()).map(p => p.url)));
check('…and takes it out of the archive', !!(await until(async () => !(await archived()).includes(url('bar-old')), 4000)),
    JSON.stringify(await archived()));

before = (await pages()).length;
check('a typed address opens in a new Today tab',
    await choose(`${site}/a.html`, r => r.title === `${site}/a.html`) &&
    !!(await until(async () => (await pages()).length === before + 1 &&
        await evaluate(sidebar, `${APP}.view_.today.some(t => t.url === '${site}/a.html' && t.isActive)`))),
    JSON.stringify({pages: (await pages()).map(p => p.url), rows: await rows()}));

await openBar();
await press(await bar(), 'Escape');
check('Esc closes the bar', !!(await until(async () => !(await shown()))));

// Cmd+L on the page: the bar edits its address.
const page = (await pages()).find(p => p.url === `${site}/a.html`);
await press(page, 'l', 4);
check('Cmd+L shows the bar with the page\'s address in it, selected', !!(await until(async () =>
    (await shown()) && await evaluate(await bar(), `(() => { const i = ${BAR}.querySelector('#input');
      return i.value === '${site}/a.html' && i.selectionStart === 0 && i.selectionEnd === i.value.length; })()`))),
    String(await bar() && await evaluate(await bar(), `${BAR}.querySelector('#input').value`)));
before = (await pages()).length;
await type(`${site}/b.html`);
await rowsFor(`${site}/b.html`, r => r[0]?.url === `${site}/b.html`);
await press(await bar(), 'Enter');
check('…and Enter opens the address in that tab', !!(await until(async () =>
    (await targets()).some(t => t.id === page.id && t.url === `${site}/b.html`))) &&
    (await pages()).length === before, JSON.stringify((await pages()).map(p => p.url)));
process.exit(results.every(Boolean) ? 0 : 1);
