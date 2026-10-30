// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A link from an Essential or a pinned tab to another site opens in Glance
// over it, and the tab stays on its page, as Zen's
// zen.glance.open-essential-external-links and Arc's Peek do. The fixture
// server answers as 127.0.0.1 and as localhost: two hosts, one server.
const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const other = site.replace('127.0.0.1', 'localhost');
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
async function evaluate(target, expression, userGesture = false) {
  const m = await send(target, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true, userGesture});
  if (m.result?.exceptionDetails) throw new Error(JSON.stringify(m.result.exceptionDetails).slice(0, 300));
  return m.result?.result?.value;
}
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);
const loaded = async url => {
  const page = await until(() => pageAt(url));
  return page && await until(async () => (await evaluate(page, 'document.readyState')) === 'complete' && page);
};
// A link followed as the user's click follows it: with a user gesture.
const click = (page, selector) => evaluate(page, `document.querySelector('${selector}').click(), true`, true);
// Esc as macOS sends it: a raw key down, with its character (headless, a key
// without one that the page lets through goes to the app menu, whose first
// item is About).
async function escape(page) {
  const k = {key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 53};
  await send(page, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', ...k, text: '\u001b', unmodifiedText: '\u001b'});
  // The glance it closes is gone by then.
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyUp', ...k}).catch(() => {});
}
async function cmdClick(page, selector) {
  const r = await evaluate(page, `(() => { const r = document.querySelector('${selector}').getBoundingClientRect(); return {x: r.x + 20, y: r.y + r.height / 2}; })()`);
  const at = {x: r.x, y: r.y, button: 'left', clickCount: 1, modifiers: 4};
  await send(page, 'Input.dispatchMouseEvent', {type: 'mouseMoved', x: r.x, y: r.y, button: 'none'});
  await send(page, 'Input.dispatchMouseEvent', {type: 'mousePressed', ...at});
  await send(page, 'Input.dispatchMouseEvent', {type: 'mouseReleased', ...at});
}
// Cmd+O expands a glance into a tab (Zen's zen-glance-expand).
async function cmdO(page) {
  const key = {key: 'o', code: 'KeyO', windowsVirtualKeyCode: 79, nativeVirtualKeyCode: 31, modifiers: 4};
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyDown', ...key, text: 'o', unmodifiedText: 'o'});
  await send(page, 'Input.dispatchKeyEvent', {type: 'keyUp', ...key});
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
const glanceOf = async (url, parent) => (await glances()).some(g => g.url === url && g.parent === parent);
const state = async () => JSON.stringify({today: await today(), glances: await glances(), active: await active(),
                                          pages: (await targets()).filter(t => t.type === 'page').map(t => t.url)});

// The Essential.
const essentialUrl = `${site}/essentials.html?e`;
const start = (await targets()).find(t => t.type === 'page' && t.url === 'about:blank');
await send(start, 'Page.navigate', {url: essentialUrl});
let essential = await loaded(essentialUrl);
await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${APP}.tabs_.find(t => t.url === '${essentialUrl}').id, '', 0, true), true`);
check('setup: the page is an Essential',
    !!(await until(() => evaluate(sidebar, `!!${ROOT}.querySelector('#essentials .essential[data-id]')`))));
// A Today tab, so the window keeps a page of its own.
await send(await browserWs(), 'Target.createTarget', {url: `${site}/essentials.html?t`, background: true});
const todayPage = await loaded(`${site}/essentials.html?t`);
await send(await browserWs(), 'Target.activateTarget', {targetId: essential.id});
await sleep(300);

// 1. A link to the same site stays in the tab.
await click(essential, '#same');
check('a link to the same site navigates the Essential in place',
    !!(await until(() => pageAt(`${site}/icon.html?same-e`))) && !(await pageAt(essentialUrl)), await state());
await send(await pageAt(`${site}/icon.html?same-e`), 'Page.navigate', {url: essentialUrl});
essential = await loaded(essentialUrl);

// 2. A link to another site opens in Glance; the Essential stays.
const innerUrl = `${other}/essentials.html?inner-e`;
await click(essential, '#other');
const inner = await loaded(innerUrl);
check('a link to another site opens in Glance over the Essential',
    !!inner && !!(await until(() => glanceOf(innerUrl, essentialUrl))), await state());
check('…the Essential stays on its page', !!(await pageAt(essentialUrl)), await state());
check('…no Today row for it', !(await today()).includes(innerUrl), await state());
check('…and the glance is the active tab', (await active()) === innerUrl, await state());
check('…with a chip on the Essential',
    !!(await until(() => evaluate(sidebar, `!!${ROOT}.querySelector('#essentials .essential .glance-chip')`))),
    await state());

// 3. Links inside the glance stay in it, even to another site.
await click(inner, '#home');
check('a link inside the glance to another site stays in the glance',
    !!(await until(() => glanceOf(`${site}/icon.html?home-inner-e`, essentialUrl))) &&
        !!(await pageAt(essentialUrl)), await state());
await escape(await pageAt(`${site}/icon.html?home-inner-e`));
check('Esc closes the glance, back to the Essential',
    !!(await until(async () => !(await pageAt(`${site}/icon.html?home-inner-e`)) && (await active()) === essentialUrl)),
    await state());

// 4. target=_blank and window.open to another site open in Glance too.
await click(essential, '#blank');
check('a target=_blank link to another site opens in Glance',
    !!(await until(() => glanceOf(`${other}/icon.html?blank-e`, essentialUrl))) &&
        !(await today()).includes(`${other}/icon.html?blank-e`), await state());
await escape(await until(() => pageAt(`${other}/icon.html?blank-e`)));
await until(async () => !(await pageAt(`${other}/icon.html?blank-e`)));
await click(essential, '#open');
check('window.open to another site opens in Glance',
    !!(await until(() => glanceOf(`${other}/icon.html?open-e`, essentialUrl))), await state());
await escape(await until(() => pageAt(`${other}/icon.html?open-e`)));
await until(async () => !(await pageAt(`${other}/icon.html?open-e`)));

// 5. Cmd+click is the user's own choice of a background tab.
await cmdClick(essential, '#background');
const background = `${other}/icon.html?background-e`;
check('Cmd+click opens an ordinary background tab, not a glance',
    !!(await until(async () => (await today()).includes(background))) &&
        !(await glances()).some(g => g.url === background) && (await active()) === essentialUrl, await state());
const backgroundPage = await pageAt(background);
if (backgroundPage) {
  await send(await browserWs(), 'Target.closeTarget', {targetId: backgroundPage.id});
}

// 6. A form posts in place.
await click(essential, '#post button');
check('a POST to another site stays in the tab',
    !!(await until(async () => (await targets()).some(t => t.type === 'page' && t.url === `${other}/icon.html?post-e`))) &&
        !(await glances()).length, await state());
await send(await pageAt(`${other}/icon.html?post-e`), 'Page.navigate', {url: essentialUrl});
essential = await loaded(essentialUrl);

// 7. A download from another site leaves no empty glance behind.
await click(essential, '#download');
await sleep(2500);
check('a download link to another site leaves no glance behind',
    !(await glances()).length && (await active()) === essentialUrl, await state());

// 8. A Today tab follows its links as before.
await send(await browserWs(), 'Target.activateTarget', {targetId: todayPage.id});
await sleep(300);
await click(todayPage, '#other');
check('in a Today tab a link to another site navigates in place',
    !!(await until(() => pageAt(`${other}/essentials.html?inner-t`))) && !(await pageAt(`${site}/essentials.html?t`)) &&
        !(await glances()).length, await state());

// 9. A pinned item that is not an Essential: Glance as well.
const pinnedUrl = `${site}/essentials.html?p`;
await send(await browserWs(), 'Target.createTarget', {url: pinnedUrl});
const pinned = await loaded(pinnedUrl);
const pinnedTab = await until(() => evaluate(sidebar, `${APP}.tabs_.find(t => t.url === '${pinnedUrl}')?.id`));
check('setup: the page is a tab of the window', !!pinned && pinnedTab !== null, await state());
await evaluate(sidebar, `${APP}.proxy_.handler.pinTab(${JSON.stringify(pinnedTab)}, '', 0, false), true`);
await sleep(500);
await click(pinned, '#other');
const pinnedInner = `${other}/essentials.html?inner-p`;
check('a link from a pinned tab to another site opens in Glance',
    !!(await until(() => glanceOf(pinnedInner, pinnedUrl))) && !!(await pageAt(pinnedUrl)), await state());
check('…with a chip on its row',
    !!(await until(() => evaluate(sidebar, `!!${ROOT}.querySelector('#tree .row.pinned .glance-chip')`))), await state());

// 10. Cmd+O expands it: the first Today tab. A synthetic Cmd+O reaches the
// app menu only from the key window: headless there is none, and windowed
// the test window must be in front.
if (process.env.CROMARC_TEST_WINDOWED) {
  await send(await pageAt(pinnedInner), 'Page.bringToFront');
  await cmdO(await pageAt(pinnedInner));
  check('Cmd+O expands the glance into the first Today tab',
      !!(await until(async () => (await today())[0] === pinnedInner && !(await glances()).length)), await state());
  check('…and the chip is gone',
      !(await evaluate(sidebar, `!!${ROOT}.querySelector('.glance-chip')`)), await state());
}

process.exit(results.every(Boolean) ? 0 : 1);
