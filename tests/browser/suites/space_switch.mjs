// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A seamless space switch: as the sidebar's animation starts, the target
// space's window comes in faint over the visible one and paints (its pages
// are visible to Chrome), so the swap at the end shows it ready; a cancelled
// swipe puts it away again.
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
// The sidebar of the window whose space is `name`.
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

const first = (await sidebars())[0];
const firstName = await until(() => evaluate(first, `${APP}.spaces_.find(s => s.current)?.name`));
await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
const second = await until(() => sidebarOf('Second'), 20000);
check('setup: a second space with its own window', !!second);
// The first window is put away behind the new space's.
check('…and the first space\'s window is hidden', !!(await until(async () => await visibility(first) === 'hidden')),
    await visibility(first));

const firstId = await evaluate(second, `${APP}.spaces_.find(s => s.name === ${JSON.stringify(firstName)}).id`);
await evaluate(second, `${APP}.proxy_.handler.prepareSpaceSwitch('${firstId}'), true`);
check('prepared: the target\'s window paints ahead of the swap',
    !!(await until(async () => await visibility(first) === 'visible', 5000)), await visibility(first));
check('…while the visible space\'s window keeps painting', await visibility(second) === 'visible');

await evaluate(second, `${APP}.proxy_.handler.prepareSpaceSwitch(''), true`);
check('cancelled: the target\'s window goes away again',
    !!(await until(async () => await visibility(first) === 'hidden', 5000)), await visibility(first));

// A switch through the sidebar: animation, prepare, swap.
await evaluate(second, `${APP}.switchTo_('${firstId}'), true`);
check('a switch shows the target space\'s window',
    !!(await until(async () => await visibility(first) === 'visible', 5000)), await visibility(first));
check('…and puts the other away', !!(await until(async () => await visibility(second) === 'hidden', 5000)),
    await visibility(second));
process.exit(results.every(Boolean) ? 0 : 1);
