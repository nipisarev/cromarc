// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Swiping space by space towards the first one: each swipe lands on the
// next space, and the Library opens only on a swipe past the first space
// (zen:library/ZenLibrary.mjs, readySwipeOpenLibrary).
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
const currentName = t => evaluate(t, `${APP}.spaces_.find(s => s.current)?.name`);
const sidebarOf = async name => {
  for (const t of await sidebars()) {
    try { if (await currentName(t) === name) return t; } catch {}
  }
  return null;
};
const visibility = t => evaluate(t, 'document.visibilityState');
// The one space window on screen (a prepared one shows too, at 1%).
const shown = async () => {
  const names = [];
  for (const t of await sidebars()) {
    try { if (await visibility(t) === 'visible') names.push(await currentName(t)); } catch {}
  }
  return names;
};
const libraryOpen = async () => {
  const open = [];
  for (const t of await sidebars()) {
    try { if (await evaluate(t, `${APP}.libraryOpen_`)) open.push(await currentName(t)); } catch {}
  }
  return open;
};
// A two-finger swipe to the right, as the trackpad tracker reports it: the
// amount grows towards the previous space over ~200 ms, then ends there.
const swipe = (t, direction) => evaluate(t, `(async () => {
  const app = ${APP};
  app.onSwipe_({type: 0, amount: 0, direction: 0});
  for (let i = 1; i <= 12; i++) {
    app.onSwipe_({type: 1, amount: ${-direction} * -0.05 * i, direction: 0});
    await new Promise(r => setTimeout(r, 16));
  }
  app.onSwipe_({type: 2, amount: ${-direction} * -0.6, direction: ${direction}});
  return true;
})()`);
const swipePrevious = t => swipe(t, -1);
const swipeNext = t => swipe(t, 1);

const first = (await sidebars())[0];
await until(() => currentName(first));
await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
await until(() => sidebarOf('Second'), 20000);
const second = await sidebarOf('Second');
await evaluate(second, `${APP}.proxy_.handler.createSpace('Third', ''), true`);
const third = await until(() => sidebarOf('Third'), 20000);
check('setup: three spaces', !!third);
await until(async () => (await shown()).length === 1, 5000);

const order = await evaluate(third, `${APP}.spaces_.map(s => s.name)`);
check('setup: three spaces in the switcher', order.length === 3, JSON.stringify(order));
// Start on the last space, as in Work of Personal <-> Qase <-> Work.
const last = await sidebarOf(order[2]);
const visible = await shown();
if (visible[0] !== order[2]) {
  await evaluate(await sidebarOf(visible[0]), `${APP}.switchTo_(${JSON.stringify(
      await evaluate(last, `${APP}.spaces_.find(s => s.current).id`))}), true`);
}
check('on the last space',
    !!(await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[2]]), 5000)),
    JSON.stringify(await shown()));

await swipePrevious(last);
check('a swipe right goes to the middle space',
    !!(await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[1]]), 5000)),
    JSON.stringify(await shown()));
check('…and opens no Library', (await libraryOpen()).length === 0, JSON.stringify(await libraryOpen()));
await sleep(500);

await swipePrevious(await sidebarOf(order[1]));
check('another swipe right goes to the first space',
    !!(await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[0]]), 5000)),
    JSON.stringify(await shown()));
check('…and still opens no Library', (await libraryOpen()).length === 0, JSON.stringify(await libraryOpen()));
await sleep(500);

// Past the first space: Zen's edge action.
await swipePrevious(await sidebarOf(order[0]));
check('a swipe right past the first space opens its Library',
    !!(await until(async () => JSON.stringify(await libraryOpen()) === JSON.stringify([order[0]]), 3000)),
    JSON.stringify(await libraryOpen()));

// With the Library open a swipe moves the Library, not the spaces (Zen's
// ZenSpacesSwipe: action LIBRARY from the start).
await sleep(500);
await swipeNext(await sidebarOf(order[0]));
check('a swipe left closes the Library',
    !!(await until(async () => (await libraryOpen()).length === 0, 3000)), JSON.stringify(await libraryOpen()));
await sleep(500);
check('…and stays on the first space', JSON.stringify(await shown()) === JSON.stringify([order[0]]),
    JSON.stringify(await shown()));
await swipePrevious(await sidebarOf(order[0]));
await until(async () => (await libraryOpen()).length === 1, 3000);
await sleep(500);

// Away from the space by its button, then back by swipes: Zen's Library
// belongs to the window on screen, so it doesn't come back with the space.
const lastId = await evaluate(last, `${APP}.spaces_.find(s => s.current).id`);
await evaluate(await sidebarOf(order[0]), `${APP}.switchTo_('${lastId}'), true`);
await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[2]]), 5000);
await sleep(500);
await swipePrevious(last);
await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[1]]), 5000);
await sleep(500);
await swipePrevious(await sidebarOf(order[1]));
check('back on the first space by swipes',
    !!(await until(async () => JSON.stringify(await shown()) === JSON.stringify([order[0]]), 5000)),
    JSON.stringify(await shown()));
check('…it shows its tabs, not the Library left open there',
    (await libraryOpen()).length === 0, JSON.stringify(await libraryOpen()));
process.exit(results.every(Boolean) ? 0 : 1);
