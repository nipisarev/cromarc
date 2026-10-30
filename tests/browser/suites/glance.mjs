// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Glance, as Zen's: Option+click on a link opens it as a glance, a tab of the
// tab it came from that the sidebar does not list. Switching away hides it,
// coming back shows it as it was; Esc and Cmd+W close it, Cmd+O expands it.
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
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  // A page this very call closes (Esc on a glance) answers by closing.
  const msg = await new Promise(r => {
    const timer = setTimeout(() => r({timeout: true}), 5000);
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) { clearTimeout(timer); r(m); } };
    ws.onclose = () => { clearTimeout(timer); r({closed: true}); };
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
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);
const mouse = (t, type, x, y, modifiers = 0) => send(t, 'Input.dispatchMouseEvent',
    {type, x, y, modifiers, button: type === 'mouseMoved' ? 'none' : 'left', clickCount: type === 'mouseMoved' ? 0 : 1});
async function optionClick(page, x, y) {
  await mouse(page, 'mouseMoved', x, y);
  await sleep(400);
  await mouse(page, 'mousePressed', x, y, 1);
  await mouse(page, 'mouseReleased', x, y, 1);
}
// Esc as macOS sends it: a raw key down, with its character (headless, a key
// without one that the page lets through goes to the app menu, whose first
// item is About).
async function escape(page) {
  const k = {key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 53};
  await send(page, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', ...k, text: '\u001b', unmodifiedText: '\u001b'});
  // The glance it closes is gone by then.
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyUp', ...k}).catch(() => {});
}
async function shortcut(page, key, code, vk, mac) {
  const k = {key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: mac, modifiers: 4};
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyDown', ...k, text: key, unmodifiedText: key});
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyUp', ...k});
}

const APP = `document.querySelector('cromarc-sidebar-app')`;
const ROOT = `${APP}.shadowRoot`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const tabUrl = id => `${APP}.tabs_.find(t => t.id === ${id})?.url`;
const today = () => evaluate(sidebar,
    `[...${ROOT}.querySelectorAll('#today .row.today')].map(r => ${tabUrl('r.dataset.id')})`);
const glances = () => evaluate(sidebar,
    `(${APP}.glances_ ?? []).map(g => ({url: ${tabUrl('g.tab')}, parent: ${tabUrl('g.parent')}}))`);
const active = () => evaluate(sidebar, `${APP}.tabs_.find(t => t.isActive)?.url`);
const currentRow = () => evaluate(sidebar,
    `${tabUrl(`${ROOT}.querySelector('#today .row.today[active]')?.dataset.id`)}`);
const chip = () => evaluate(sidebar, `!!${ROOT}.querySelector('#today .row.today .glance-chip')`);
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('', 50).then(({tabs}) => tabs.map(t => t.url))`);
const state = async () => JSON.stringify({today: await today(), glances: await glances(), active: await active(),
                                          pages: (await targets()).filter(t => t.type === 'page').map(t => t.url)});

const source = `${site}/glance.html`;
const preview = `${site}/b.html`;
let tab = (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
await send(tab, 'Page.navigate', {url: source});
tab = await until(() => pageAt(source));
await sleep(800);

// Option+click where there is no link: nothing.
await optionClick(tab, 100, 300);
await sleep(800);
check('Option+click off a link opens nothing', !(await pageAt(preview)));

await optionClick(tab, 120, 80);
const glance = await until(() => pageAt(preview));
check('Option+click on a link opens it as a glance of its tab',
    !!glance && !!(await until(async () => (await glances()).some(g => g.url === preview && g.parent === source))),
    await state());
check('…the page itself stays', !!(await pageAt(source)));
check('…the sidebar lists no tab for it', JSON.stringify(await today()) === JSON.stringify([source]), await state());
check('…its tab shows a chip, and stays current', (await until(chip)) && (await currentRow()) === source,
    await state());
check('…the glance is the active tab', (await active()) === preview, await state());
const title = await until(async () => (await evaluate(glance, 'document.title')) === 'Second link');
check('…and it has loaded', !!title, await state());

// Option+click inside the glance opens nothing more (Zen's openGlance).
await evaluate(glance, `document.body.innerHTML = '<a id=x href="/c.html" style="display:block;height:200px">c</a>'; window.__mark = 7; true`);
await optionClick(glance, 60, 60);
await sleep(1000);
check('Option+click inside a glance opens nothing more',
    !(await pageAt(`${site}/c.html`)) && (await glances()).length === 1, await state());

// Away and back: the same glance, not reloaded.
await send(await browserWs(), 'Target.createTarget', {url: `${site}/a.html`});
await until(async () => (await active()) === `${site}/a.html`);
check('switching to another tab hides the glance', (await active()) === `${site}/a.html` &&
    (await glances()).length === 1, await state());
await evaluate(sidebar, `${ROOT}.querySelector('#today .row.today[data-id="' + ${APP}.tabs_.find(t => t.url === '${source}').id + '"]').click(), true`);
check('clicking its tab shows the glance again', !!(await until(async () => (await active()) === preview)),
    await state());
check('…as it was, not reloaded', (await evaluate(await pageAt(preview), 'window.__mark')) === 7);

// Esc closes it; nothing goes to the archive for a glance the user closed.
await escape(await pageAt(preview));
check('Esc closes the glance, back to its tab',
    !!(await until(async () => !(await pageAt(preview)) && (await active()) === source)), await state());
check('…and puts nothing in the archive', !(await archived()).includes(preview), JSON.stringify(await archived()));

// Esc in a text field asks first, as Zen's: the glance stays, and Esc again
// while its close button asks closes it.
await optionClick(await pageAt(source), 120, 80);
await until(async () => (await active()) === preview);
const typing = await until(() => pageAt(preview));
check('a field in the glance has the focus', await evaluate(typing, `(() => {
  const field = document.createElement('input');
  document.body.append(field);
  field.focus();
  return document.activeElement === field;
})()`));
await sleep(300);
await escape(typing);
await sleep(800);
check('Esc in a text field leaves the glance, asking first',
    !!(await pageAt(preview)) && (await active()) === preview, await state());
await escape(typing);
check('…and Esc again closes it',
    !!(await until(async () => !(await pageAt(preview)) && (await active()) === source)), await state());

// A synthetic Cmd+W or Cmd+O reaches the app menu only from the key window:
// headless there is none, and windowed the test window must be in front.
if (process.env.CROMARC_TEST_WINDOWED) {
  // Cmd+W closes only the glance.
  await optionClick(await pageAt(source), 120, 80);
  await until(async () => (await active()) === preview);
  await send(await pageAt(preview), 'Page.bringToFront');
  await shortcut(await pageAt(preview), 'w', 'KeyW', 87, 13);
  check('Cmd+W closes only the glance',
      !!(await until(async () => !(await pageAt(preview)) && (await active()) === source)) &&
      !!(await pageAt(source)), await state());

  // Cmd+O expands it: an ordinary tab right after its Today tab.
  await optionClick(await pageAt(source), 120, 80);
  await until(async () => (await active()) === preview);
  await send(await pageAt(preview), 'Page.bringToFront');
  await shortcut(await pageAt(preview), 'o', 'KeyO', 79, 31);
  check('Cmd+O expands the glance into a tab right after its own',
      !!(await until(async () => {
        const rows = await today();
        return rows[rows.indexOf(source) + 1] === preview && !(await glances()).length;
      })), await state());
  check('…its tab no longer shows a chip', !(await chip()), await state());
}

// Closing a tab closes its glance, into the archive: nothing is lost.
await optionClick(await pageAt(source), 120, 160);
await until(async () => (await active()) === preview && (await glances()).length === 1);
const previews = async () => (await targets()).filter(t => t.url === preview).length;
const withGlance = await previews();
await send(await browserWs(), 'Target.closeTarget', {targetId: (await pageAt(source)).id});
check('closing a tab closes its glance',
    !!(await until(async () => !(await glances()).length && (await previews()) === withGlance - 1)),
    await state());
check('…and the glance goes to the archive',
    !!(await until(async () => (await archived()).includes(preview))), JSON.stringify(await archived()));

// Ordinary clicks are untouched: the link is followed.
const plain = (await targets()).find(t => t.type === 'page' && t.url === `${site}/a.html`);
await send(plain, 'Page.navigate', {url: source});
const again = await until(() => pageAt(source));
await sleep(600);
await mouse(again, 'mouseMoved', 120, 80);
await mouse(again, 'mousePressed', 120, 80, 0);
await mouse(again, 'mouseReleased', 120, 80, 0);
check('a plain click still follows the link', !!(await until(async () => (await active()) === preview &&
    !(await glances()).length)), await state());
process.exit(results.every(Boolean) ? 0 : 1);
