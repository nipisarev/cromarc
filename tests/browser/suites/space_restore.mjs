// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A space whose window was closed comes back in one window with its tabs, as
// in Zen: on a switch, as a warm window, and from the Dock. Here two spaces
// are left out of the startup restore, as when their windows were closed
// before the quit (a second, restored window opened over the first,
// 29.10.2026). The runner restarts the browser (tooling/test-browser).
import {execFileSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
const app = process.env.CROMARC_TEST_APP;
const tools = process.env.CROMARC_TEST_TOOLS;
const phase = Number(process.env.CROMARC_TEST_PHASE ?? 1);
const profileDir = path.dirname(process.env.CROMARC_TEST_DOWNLOADS);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
// Empty once the browser is gone.
const targets = async () => {
  try {
    return await (await fetch(`${base}/json/list`)).json();
  } catch {
    return [];
  }
};
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  // A target gone meanwhile (a window the key just closed) never opens.
  const opened = await Promise.race([new Promise(r => {
    ws.onopen = () => r(true);
    ws.onerror = ws.onclose = () => r(false);
  }), sleep(5000).then(() => false)]);
  if (!opened) {
    ws.close();
    return {closed: true};
  }
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  // A page not loaded yet (a tab in a window never shown) never answers.
  const msg = await Promise.race([new Promise(r => {
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); };
    ws.onclose = () => r({closed: true});
  }), sleep(5000).then(() => ({timedOut: true}))]);
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
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };
const pgrep = pattern => {
  try {
    return execFileSync('pgrep', ['-f', pattern]).toString().split('\n').filter(Boolean);
  } catch {
    return [];
  }
};
const titles = p => execFileSync(`${tools}/wintitle`, [p]).toString().split('\n').filter(Boolean);
const APP = `document.querySelector('cromarc-sidebar-app')`;
const url = name => `${site}/icon.html?${name}`;
const sidebars = async () => (await targets()).filter(t => t.url.startsWith('chrome://cromarc-sidebar'));
const spaceOf = t => evaluate(t, `${APP}.spaces_.find(s => s.current)?.name`);
// The sidebars of the windows whose space is `name`, one per window.
const sidebarsOf = async name => {
  const found = [];
  for (const t of await sidebars()) {
    try {
      if (await spaceOf(t) === name) found.push(t);
    } catch {}
  }
  return found;
};
const tabsOf = t => evaluate(t, `${APP}.tabs_.map(t => t.url).sort()`);
const visibility = t => evaluate(t, 'document.visibilityState');
// A page that answers: the selected tab of its window (the others wait
// unloaded).
const loadedPage = async prefix => {
  for (const t of (await targets()).filter(t => t.type === 'page' && t.url.startsWith(prefix))) {
    try {
      if (await evaluate(t, 'document.readyState') === 'complete') return t;
    } catch {}
  }
  return null;
};
// Windows on screen, by CGWindowList: headless has none, and there a window
// never shown may still say "visible".
const onScreen = () => execFileSync(`${tools}/onscreen`, [pid]).toString().split('\n').filter(Boolean);
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
// What every window holds, by space, for a failure's detail.
const windowsNow = async () => {
  const all = [];
  for (const t of await sidebars()) {
    try {
      all.push([await spaceOf(t), await tabsOf(t)]);
    } catch {}
  }
  return JSON.stringify(all);
};
const newTabs = async () => (await targets()).filter(t => t.type === 'page' && t.url.startsWith('chrome://newtab'));

if (phase === 1) {
  const first = await until(async () => (await sidebars())[0]);
  const firstName = await until(() => spaceOf(first));
  await evaluate(first, `${APP}.proxy_.handler.openUrl('${url('first-a')}', false), true`);
  await until(async () => (await tabsOf(first)).includes(url('first-a')));
  for (const page of (await targets()).filter(t => t.type === 'page' && t.url === 'about:blank')) {
    await fetch(`${base}/json/close/${page.id}`);
  }
  // A second window of the same space, as Cmd+N opens.
  await send(await browserWs(), 'Target.createTarget', {url: url('first-b'), newWindow: true});
  const firstWindows = await until(async () => (await sidebarsOf(firstName)).length === 2 &&
      await sidebarsOf(firstName));

  await evaluate(first, `${APP}.proxy_.handler.createSpace('Second', ''), true`);
  const second = await until(async () => (await sidebarsOf('Second'))[0], 20000);
  await evaluate(second, `${APP}.proxy_.handler.openUrl('${url('second-a')}', false), true`);
  await evaluate(second, `${APP}.proxy_.handler.createSpace('Third', ''), true`);
  const third = await until(async () => (await sidebarsOf('Third'))[0], 20000);
  await evaluate(third, `${APP}.proxy_.handler.openUrl('${url('third-a')}', false), true`);
  await until(async () => (await tabsOf(second)).includes(url('second-a')) &&
      (await tabsOf(third)).includes(url('third-a')));
  // Each window holds only its pages: the new tabs go.
  for (const page of await newTabs()) {
    await fetch(`${base}/json/close/${page.id}`);
  }
  await until(async () => (await newTabs()).length === 0);
  const secondId = await evaluate(third, `${APP}.spaces_.find(s => s.name === 'Second').id`);
  await evaluate(third, `${APP}.switchTo_('${secondId}'), true`);
  const shown = await until(async () => await visibility(second) === 'visible' && await visibility(third) === 'hidden');
  const setup = {
    first: firstWindows && (await Promise.all(firstWindows.map(tabsOf))).flat().sort(),
    second: await tabsOf(second),
    third: await tabsOf(third),
    shown,
  };
  if (!same(setup.first, [url('first-a'), url('first-b')].sort()) || !same(setup.second, [url('second-a')]) ||
      !same(setup.third, [url('third-a')]) || !shown) {
    check('setup: three spaces, the first in two windows, Second in front', false,
        JSON.stringify({setup, windows: await windowsNow()}));
    process.exit(1);
  }
  // The session is written on a timer; the quit writes what is left.
  await sleep(3000);
  const ws = new WebSocket(await browserWs());
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Browser.close'}));
  for (let i = 0; i < 60 && alive(); i++) {
    await sleep(500);
  }
  if (alive()) {
    process.exit(1);
  }
  // Only Second was open at the quit: the startup restore brings back its
  // window alone, and the other spaces keep their last sessions.
  const file = path.join(profileDir, 'Local State');
  const state = JSON.parse(fs.readFileSync(file, 'utf8'));
  const dir = Object.entries(state.profile.info_cache).find(([, info]) => info.name === 'Second')?.[0];
  if (!dir) {
    check('setup: Second has a profile', false, JSON.stringify(Object.keys(state.profile.info_cache)));
    process.exit(1);
  }
  state.profile.last_active_profiles = [dir];
  state.profile.last_used = dir;
  // With two profiles or more Chromium asks which to open at the start,
  // unless "Show on startup" in its picker was unchecked.
  state.profile.show_picker_on_startup = false;
  fs.writeFileSync(file, JSON.stringify(state));
  process.exit(75);
}

const second = await until(async () => (await sidebarsOf('Second'))[0]);
const firstName = second && await evaluate(second, `${APP}.spaces_.find(s => s.id === 'Default').name`);
check('setup: only Second\'s window comes back at the start', !!second && (await sidebars()).length === 1,
    await windowsNow());

// (a) A switch to a space the startup restore did not bring back.
await evaluate(second, `${APP}.switchTo_('Default'), true`);
await until(async () => (await sidebarsOf(firstName)).length > 0);
// Its last session is read and restored after the window opens.
await sleep(4000);
const firstWindows = await sidebarsOf(firstName);
check('a switch to a space whose window was closed shows one window for it', firstWindows.length === 1,
    await windowsNow());
check('…holding its tabs from both its saved windows, and no new tab',
    firstWindows.length === 1 && same(await tabsOf(firstWindows[0]), [url('first-a'), url('first-b')].sort()),
    await windowsNow());

// (b) The warm window of the other one, half a minute after the start.
const third = await until(async () => (await sidebarsOf('Third')).length > 0, 45000);
await sleep(4000);
const thirds = await sidebarsOf('Third');
check('a warm window of such a space is one window', !!third && thirds.length === 1, await windowsNow());
check('…holding its tabs', thirds.length === 1 && same(await tabsOf(thirds[0]), [url('third-a')]),
    await windowsNow());
if (process.env.CROMARC_TEST_WINDOWED) {
  check('…and it stays hidden: one window on screen', onScreen().length === 1, JSON.stringify(onScreen()));
} else {
  console.log('note: that the warm window stays hidden is seen in a windowed run (CROMARC_TEST_WINDOWED=1)');
}

// (c) Closing one of two windows on screen.
const before = (await sidebars()).length;
const extra = (await send(await browserWs(), 'Target.createTarget', {url: url('extra'), newWindow: true})).result?.targetId;
await until(async () => (await sidebars()).length === before + 1);
await send(await browserWs(), 'Target.closeTarget', {targetId: extra});
await until(async () => !(await targets()).some(t => t.id === extra));
await sleep(2000);
check('closing one of two windows on screen leaves the hidden ones',
    (await sidebarsOf('Second')).length === 1 && (await sidebarsOf('Third')).length === 1 &&
    (await sidebarsOf(firstName)).length === 1, await windowsNow());

// (d) Closing the last window, then the Dock icon. Cmd+Shift+W closes the
// window whole, so Chromium's session keeps its tabs; headless, shortcuts
// do not reach the browser, and the tabs close one by one, each leaving the
// session.
const windowed = !!process.env.CROMARC_TEST_WINDOWED;
if (windowed) {
  const shortcut = {key: 'W', code: 'KeyW', windowsVirtualKeyCode: 87, nativeVirtualKeyCode: 13, modifiers: 4 | 8};
  const firstPage = await until(() => loadedPage(url('first-')));
  if (firstPage) {
    await send(firstPage, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', ...shortcut, text: 'W', unmodifiedText: 'W'});
    await send(firstPage, 'Input.dispatchKeyEvent', {type: 'keyUp', ...shortcut});
  }
} else {
  for (const page of (await targets()).filter(t => t.type === 'page' && t.url.startsWith(url('first-')))) {
    await send(await browserWs(), 'Target.closeTarget', {targetId: page.id});
    await until(async () => !(await targets()).some(t => t.id === page.id));
  }
}
const empty = await until(async () => (await sidebars()).length === 0);
check('closing the last window on screen closes every space\'s window', !!empty && alive(),
    JSON.stringify({alive: alive(), windows: await windowsNow()}));
// LaunchServices sends a running app a reopen, as a click on its Dock icon
// does. Should it start a browser of its own instead, that one gets a
// profile in this run's folder, never the user's.
const stray = path.join(profileDir, 'stray');
const daily = pgrep('^/Applications/Cromarc.app/Contents/MacOS/Cromarc');
const dailyBefore = JSON.stringify(daily.map(titles));
execFileSync('open', ['-g', '-a', app, '--args', `--user-data-dir=${stray}`, '--headless=new',
  '--no-first-run', '--use-mock-keychain', '--disable-features=MacAppCodeSignClone']);
const reopened = await until(async () => (await sidebars()).length > 0);
const stays = pgrep(`user-data-dir=${stray}`);
if (stays.length) {
  execFileSync('kill', stays);
}
await sleep(4000);
const back = await sidebarsOf(firstName);
check('the Dock icon reaches this browser, not a new one or the user\'s', !!reopened && stays.length === 0 &&
    JSON.stringify(daily.map(titles)) === dailyBefore, JSON.stringify({stays, daily}));
check('…and shows the last active space in one window', back.length === 1, await windowsNow());
if (windowed) {
  check('…with its tabs', back.length === 1 && same(await tabsOf(back[0]), [url('first-a'), url('first-b')].sort()),
      await windowsNow());
} else {
  console.log('note: that the Dock brings the tabs back is seen in a windowed run (CROMARC_TEST_WINDOWED=1)');
}
process.exit(results.every(Boolean) ? 0 : 1);
