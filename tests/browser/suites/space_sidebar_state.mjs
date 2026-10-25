// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Zen's sidebar belongs to its window, whichever space it shows: compact or
// not. Each space here has a window of its own; they share it.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => {
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); };
    ws.onclose = () => r({closed: true});
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
const sidebars = async () => (await (await fetch(`${base}/json/list`)).json())
    .filter(t => t.url.startsWith('chrome://cromarc-sidebar'));
const sidebarOf = async name => {
  for (const t of await sidebars()) {
    try {
      if (await evaluate(t, `${APP}.spaces_.find(s => s.current)?.name`) === name) {
        return t;
      }
    } catch {}
  }
  return null;
};
const visibility = t => evaluate(t, 'document.visibilityState');
// What one sees of the sidebar: the room it leaves the page beside it (a
// collapsed column clips its own page, which keeps its width).
const pages = async () => (await (await fetch(`${base}/json/list`)).json()).filter(t => t.type === 'page');
const pageTitled = async title => (await pages()).find(t => t.title === title);
const pageWidth = async title => evaluate(await pageTitled(title), 'innerWidth');
const openPage = (sidebar, title) => evaluate(sidebar,
    `${APP}.proxy_.handler.openUrl('data:text/html,<title>${title}</title>${title}', false), true`);
// A compact sidebar has no width, and its page counts as hidden: the window's
// own page tells which window is on screen.
const pageVisibility = async title => visibility(await pageTitled(title));
const sample = async (fn, ms) => {
  const values = [];
  const end = Date.now() + ms;
  while (Date.now() < end) {
    values.push(await fn());
    await sleep(50);
  }
  return values;
};

const first = (await sidebars())[0];
const firstName = await until(() => evaluate(first, `${APP}.spaces_.find(s => s.current)?.name`));
await openPage(first, 'first-page');
await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
const second = await until(() => sidebarOf('Second'), 20000);
await openPage(second, 'second-page');
const shown = await until(() => pageWidth('second-page'));
check('setup: a second space with its own window', !!second && !!(await pageTitled('first-page')) && shown > 0);
const isCompact = w => w >= shown + 100;
const isShown = w => w <= shown + 2;

await evaluate(second, `${APP}.proxy_.handler.toggleCompactMode(), true`);
check('compact in one space\'s window', !!(await until(async () => isCompact(await pageWidth('second-page')))),
    String(await pageWidth('second-page')));

// Brought over the visible window ahead of a switch, the other space's window
// is compact from its first frame.
const firstId = await evaluate(second, `${APP}.spaces_.find(s => s.name === ${JSON.stringify(firstName)}).id`);
await evaluate(second, `${APP}.proxy_.handler.prepareSpaceSwitch('${firstId}'), true`);
await until(async () => await pageVisibility('first-page') === 'visible', 5000);
const prepared = await sample(() => pageWidth('first-page'), 500);
check('…is compact in the other space\'s window too', prepared.every(isCompact), JSON.stringify([...new Set(prepared)]));
await evaluate(second, `${APP}.proxy_.handler.prepareSpaceSwitch(''), true`);
await until(async () => await pageVisibility('first-page') === 'hidden', 5000);

// The switch: the sidebar never comes out on the way.
await evaluate(second, `${APP}.switchTo_('${firstId}'), true`);
const switching = await sample(() => pageWidth('first-page'), 3000);
check('a switch keeps the sidebar compact all the way', await pageVisibility('first-page') === 'visible' &&
    switching.every(isCompact), `${JSON.stringify([...new Set(switching)])} ${await pageVisibility('first-page')}`);

await evaluate(first, `${APP}.proxy_.handler.toggleCompactMode(), true`);
check('back from compact in one window', !!(await until(async () => isShown(await pageWidth('first-page')))),
    String(await pageWidth('first-page')));
const secondId = await evaluate(first, `${APP}.spaces_.find(s => s.name === 'Second').id`);
// Headless, a window off screen plays the animation in a few slow frames
// (947 for 600 ms, then 852); one on screen is done by now.
await until(async () => isShown(await pageWidth('second-page')), 5000);
await evaluate(first, `${APP}.proxy_.handler.prepareSpaceSwitch('${secondId}'), true`);
await until(async () => await pageVisibility('second-page') === 'visible', 5000);
const preparedBack = await sample(() => pageWidth('second-page'), 500);
check('…and in the other', preparedBack.every(isShown), JSON.stringify([...new Set(preparedBack)]));
await evaluate(first, `${APP}.proxy_.handler.prepareSpaceSwitch(''), true`);
await until(async () => await pageVisibility('second-page') === 'hidden', 5000);

// A space made while compact starts compact, without coming out first.
await evaluate(first, `${APP}.proxy_.handler.toggleCompactMode(), true`);
await until(async () => isCompact(await pageWidth('first-page')));
const before = new Set((await pages()).map(t => t.id));
await evaluate(first, `${APP}.proxy_.handler.createSpace('Third', ''), true`);
const thirdPage = await until(async () => (await pages()).find(t => !before.has(t.id)), 20000);
const third = thirdPage && await sample(async () => evaluate(thirdPage, 'innerWidth'), 2000);
check('a new space\'s window starts compact', !!third && third.every(isCompact), JSON.stringify([...new Set(third ?? [])]));
process.exit(results.every(Boolean) ? 0 : 1);
