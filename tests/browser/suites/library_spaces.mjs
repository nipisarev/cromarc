// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Library's Spaces (zen:library/sections/ZenLibrarySpacesSection.mjs):
// a card per space, renamed and given an icon in place, moved by its
// handle, opened on a tab of another space; the Library as wide as they
// need.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
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
const targets = async () => (await fetch(`${base}/json/list`)).json();
const sidebars = async () => (await targets()).filter(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const LIB = `${APP}.shadowRoot.querySelector('cromarc-library')`;
const IN = selector => `${LIB}.shadowRoot.querySelector(${JSON.stringify(selector)})`;
const ALL = selector => `[...${LIB}.shadowRoot.querySelectorAll(${JSON.stringify(selector)})]`;
const S = 'cromarc-library-spaces';
const currentName = t => evaluate(t, `${APP}.spaces_.find(s => s.current)?.name`);
const visibility = t => evaluate(t, 'document.visibilityState');
const settled = (t, open) => until(async () => evaluate(t,
    `${APP}.libraryOpen_ === ${open} && ${APP}.libraryProgress_ === ${open ? 1 : 0}`));
const openSpaces = async t => {
  await evaluate(t, `${APP}.shadowRoot.querySelector('#library-button').click()`);
  await settled(t, true);
  await evaluate(t, `${IN('.tab[data-section="spaces"]')}.click(), true`);
  return until(() => evaluate(t, `${ALL(`${S} .space-card`)}.length`));
};
// A real mouse in the page, through DevTools: pointer capture needs one.
const mouse = (t, type, x, y) => send(t, 'Input.dispatchMouseEvent',
    {type, x, y, button: 'left', buttons: type === 'mouseReleased' ? 0 : 1, clickCount: 1});

const first = (await sidebars())[0];
await until(() => currentName(first));
await send((await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl,
    'Target.createTarget', {url: `${site}/a.html`});
await until(async () => (await evaluate(first, `${APP}.tabs_.length`)) >= 2);

check('Spaces shows a card for the one space', (await openSpaces(first)) === 1);
const firstName = await currentName(first);
check('…with its name and its tabs', await evaluate(first, `${IN(`${S} .space-card-name`)}.textContent`) === firstName &&
    !!(await until(async () => (await evaluate(first, `${ALL(`${S} .space-row-title`)}.map(n => n.textContent)`)).includes('Little test'))));
check('the Library widens for the cards and the add button', !!(await until(async () =>
    (await evaluate(first, `${APP}.libraryWidth_`)) > 418 && (await evaluate(first, 'innerWidth')) > 418)),
    `${await evaluate(first, `${APP}.libraryWidth_`)} / ${await evaluate(first, 'innerWidth')}`);

await evaluate(first, `${IN(`${S} .space-card-name`)}.click(), true`);
await until(() => evaluate(first, `!!${IN(`${S} .space-card-name-input`)}`));
await evaluate(first, `(() => { const i = ${IN(`${S} .space-card-name-input`)}; i.value = 'Работа'; i.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'})); return true; })()`);
check('the name renames the space in place', !!(await until(async () => (await currentName(first)) === 'Работа')));

await evaluate(first, `${IN(`${S} .space-card-icon`)}.click(), true`);
check('the icon opens the icon picker on the card', !!(await until(() => evaluate(first, `!!${IN(`${S} cromarc-icon-picker`)}`))));
await evaluate(first, `${IN(`${S} cromarc-icon-picker`)}.dispatchEvent(new CustomEvent('icon-picked', {detail: '🚀'})), true`);
check('…which sets it', !!(await until(async () => (await evaluate(first, `${APP}.spaces_.find(s => s.current).icon`)) === '🚀')));

// A second space; its window takes the first one's place.
await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
const second = await until(async () => {
  for (const t of await sidebars()) {
    try { if (await currentName(t) === 'Second') return t; } catch {}
  }
  return null;
}, 20000);
await until(async () => await visibility(second) === 'visible' && await visibility(first) === 'hidden');
check('setup: two spaces', (await openSpaces(second)) === 2);
const order = async () => evaluate(second, `${APP}.spaces_.map(s => s.name).join()`);
const before = await order();

// Drag the first card's handle past the second card.
const handle = await evaluate(second, `(() => { const r = ${ALL(`${S} .space-card-handle`)}[0].getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; })()`);
await mouse(second, 'mouseMoved', handle[0], handle[1]);
await mouse(second, 'mousePressed', handle[0], handle[1]);
for (let dx = 20; dx <= 300; dx += 20) {
  await mouse(second, 'mouseMoved', handle[0] + dx, handle[1]);
  await sleep(16);
}
check('a card follows its handle', parseFloat(await evaluate(second, `${ALL(`${S} .space-card`)}[0].style.translate`)) > 100);
await mouse(second, 'mouseReleased', handle[0] + 300, handle[1]);
const after = await until(async () => { const o = await order(); return o !== before && o; });
check('letting go reorders the spaces', !!after && after === before.split(',').reverse().join(),
    `${before} → ${await order()}`);

// A tab of the other space brings that space's window up.
const otherRow = `${ALL(`${S} .space-card`)}.find(c => c.querySelector('.space-card-name').textContent === 'Работа').querySelector('.space-row[data-url]')`;
await until(() => evaluate(second, `!!${otherRow}`));
await evaluate(second, `${otherRow}.click(), true`);
check('a tab of another space opens that space', !!(await until(async () =>
    await visibility(first) === 'visible' && await visibility(second) === 'hidden')));
check('…the Library shut in the window left', await evaluate(second, `!${APP}.libraryOpen_ && !${APP}.libraryShown_`));
process.exit(results.every(Boolean) ? 0 : 1);
