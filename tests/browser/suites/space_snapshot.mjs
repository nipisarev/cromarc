// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// While switching, another space's strip is drawn from its snapshot, then
// that space's window takes the place of this one: the strip must look as the
// window's own sidebar does, favicons included, or the swap shows a seam (the
// favicons blinked: chrome://favicon2 asks the profile of the page showing
// it, and each space has a profile of its own).
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
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
// What one sees of a strip: its items in order, each with where it sits
// in the strip, its state and its favicon.
const stripOf = (sidebar, live, spaceName) => evaluate(sidebar, `(() => {
  const root = ${APP}.shadowRoot;
  const strip = ${live} ? root.querySelector('#sidebar') :
      [...root.querySelectorAll('.sidebar.snapshot')].find(s =>
          s.querySelector('.space-name')?.textContent.trim() === ${JSON.stringify(spaceName)});
  if (!strip) return null;
  const top = strip.getBoundingClientRect().top;
  return [...strip.querySelectorAll('.essential, .row, .separator')].map(e => ({
    kind: e.className.replace(/\\s+/g, ' ').trim(),
    title: e.querySelector('.title')?.textContent.trim() ?? e.title,
    top: Math.round(e.getBoundingClientRect().top - top),
    state: e.getAttribute('state'),
    active: e.hasAttribute('active'),
    favicon: (e.querySelector('.favicon.current') ?? e.querySelector('.favicon'))
        ?.style.backgroundImage ?? '',
  }));
})()`);
const tabIdOf = (sidebar, title) =>
    `${APP}.tabs_.find(t => t.title === ${JSON.stringify(title)}).id`;

const first = (await sidebars())[0];
const firstName = await until(() => evaluate(first, `${APP}.spaces_.find(s => s.current)?.name`));
for (const n of [1, 2, 3, 4, 5]) {
  await evaluate(first, `${APP}.proxy_.handler.openUrl('${site}/icon.html?${n}', false), true`);
}
check('setup: pages with favicons', !!(await until(() => evaluate(first,
    `${APP}.tabs_.filter(t => t.title.startsWith('Icon ') && t.favicon.dataUrl).length === 5`))));
// An essential, two pinned tabs (one asleep: its tab closed) and Today.
await evaluate(first, `${APP}.proxy_.handler.pinTab(${tabIdOf(first, 'Icon 1')}, '', 0, true), true`);
await evaluate(first, `${APP}.proxy_.handler.pinTab(${tabIdOf(first, 'Icon 2')}, '', 0, false), true`);
await evaluate(first, `${APP}.proxy_.handler.pinTab(${tabIdOf(first, 'Icon 3')}, '', 1, false), true`);
await until(() => evaluate(first, `${APP}.view_.rows.length === 2 && ${APP}.view_.essentials.length === 1`));
await evaluate(first, `${APP}.shadowRoot.querySelector('#tree button.close[data-tab="' + ${tabIdOf(first, 'Icon 3')} + '"]').click(), true`);
check('setup: one pinned tab asleep', !!(await until(() => evaluate(first,
    `${APP}.view_.rows.some(r => r.state === 'asleep')`))));

await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
const second = await until(() => sidebarOf('Second'), 20000);
check('setup: a second space with its own window', !!second);

// The second window's strip for the first space, as a switch draws it.
const snapshot = await until(async () => {
  await evaluate(second, `${APP}.loadSnapshots_().then(() => true)`);
  const strip = await stripOf(second, false, firstName);
  // The asleep tab's favicon comes from the first space's favicon database,
  // written as its page loaded.
  return strip?.find(e => e.state === 'asleep')?.favicon.startsWith('url("data:') && strip;
});
const live = await stripOf(first, true, firstName);
check('the snapshot draws the asleep tab\'s favicon from its own space', !!snapshot,
    JSON.stringify(await stripOf(second, false, firstName)));
const shape = strip => strip?.map(e => ({...e, favicon: e.state === 'asleep' ? '' : e.favicon}));
check('the snapshot is drawn as that space\'s own sidebar: items, places, states, favicons',
    JSON.stringify(shape(snapshot)) === JSON.stringify(shape(live)),
    `\n  live     ${JSON.stringify(shape(live))}\n  snapshot ${JSON.stringify(shape(snapshot))}`);
check('…its awake tabs with their own favicons',
    !!snapshot && snapshot.filter(e => e.title?.startsWith('Icon ') && e.state !== 'asleep')
        .every(e => e.favicon.startsWith('url("data:')),
    JSON.stringify(snapshot));
check('a snapshot does not take clicks or focus',
    await evaluate(second, `[...${APP}.shadowRoot.querySelectorAll('.sidebar.snapshot')].every(s => s.inert)`));
process.exit(results.every(Boolean) ? 0 : 1);
