// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A link from another app opens in a Little window of its own, as in Arc,
// and closing that window keeps its page in the space's archive. The link
// comes the way another app's does: LaunchServices (`open -a`).
import {execFileSync} from 'node:child_process';
import path from 'node:path';

const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
const app = process.env.CROMARC_TEST_APP;
const tools = process.env.CROMARC_TEST_TOOLS;
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
async function until(fn, ms = 10000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const pgrep = pattern => {
  try {
    return execFileSync('pgrep', ['-f', pattern]).toString().split('\n').filter(Boolean);
  } catch {
    return [];
  }
};
const titles = p => execFileSync(`${tools}/wintitle`, [p]).toString().split('\n').filter(Boolean);
// Should LaunchServices start a browser of its own instead, it gets a profile
// in this run's folder, never the user's.
const stray = path.join(path.dirname(process.env.CROMARC_TEST_DOWNLOADS), 'stray');
const openLink = url => execFileSync('open', ['-g', '-a', app, url, '--args', `--user-data-dir=${stray}`,
  '--headless=new', '--no-first-run', '--use-mock-keychain', '--disable-features=MacAppCodeSignClone']);
const daily = pgrep('^/Applications/Cromarc.app/Contents/MacOS/Cromarc');

const APP = `document.querySelector('cromarc-sidebar-app')`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const windowOf = async target => (await send(await browserWs(), 'Browser.getWindowForTarget', {targetId: target.id})).result;
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);
const spaceWindow = await windowOf(await until(() => pageAt('about:blank')));
const openUrl = `${site}/icon.html?little-open`;
const closeUrl = `${site}/icon.html?little-close`;

openLink(openUrl);
const little = await until(() => pageAt(openUrl));
const stays = pgrep(`user-data-dir=${stray}`);
if (stays.length) {
  execFileSync('kill', stays);
}
check('a link from another app reaches this browser, not a new one', !!little && stays.length === 0,
    JSON.stringify({little: !!little, stays}));
const littleWindow = little && await windowOf(little);
check('it opens in a window of its own', !!littleWindow && littleWindow.windowId !== spaceWindow.windowId,
    JSON.stringify({littleWindow, spaceWindow}));
const screen = little && await evaluate(little, `({left: screen.availLeft, width: screen.availWidth})`);
check('…900 wide, centred across the screen', !!littleWindow && littleWindow.bounds.width === 900 &&
    Math.abs(littleWindow.bounds.left + 450 - (screen.left + screen.width / 2)) <= 1,
    JSON.stringify({bounds: littleWindow?.bounds, screen}));
check('…and the space window gets no tab for it',
    !(await evaluate(sidebar, `${APP}.tabs_.some(t => t.url === '${openUrl}')`)),
    JSON.stringify(await evaluate(sidebar, `${APP}.tabs_.map(t => t.url)`)));
if (little) {
  await send(await browserWs(), 'Target.closeTarget', {targetId: little.id});
  await until(async () => !(await pageAt(openUrl)));
}

openLink(closeUrl);
const second = await until(async () => {
  const page = await pageAt(closeUrl);
  return page && await evaluate(page, `document.title === 'Icon little-close'`) && page;
});
await send(await browserWs(), 'Target.closeTarget', {targetId: second?.id});
check('closed, a Little window goes', !!second &&
    !!(await until(() => !titles(pid).includes('Icon little-close'))), JSON.stringify(titles(pid)));
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('little-close', 10).then(({tabs}) => tabs.map(t => [t.url, t.reason]))`);
check('…and its page is in the space\'s archive, as closed from a Little window',
    !!(await until(async () => (await archived()).some(([url, reason]) => url === closeUrl && reason === 'little'))),
    JSON.stringify(await archived()));

// The user's own Cromarc, if it runs, never got these links.
check('no link reached another Cromarc',
    daily.every(p => !titles(p).some(t => t.startsWith('Icon little'))) &&
    pgrep(`user-data-dir=${stray}`).length === 0, JSON.stringify(daily));
process.exit(results.every(Boolean) ? 0 : 1);
