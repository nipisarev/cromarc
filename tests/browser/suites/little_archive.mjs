// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A Little window left unused archives itself, as Little Arc windows do
// (every 6 h; half the tab limit). Run with --cromarc-idle-limit=8, so the
// Little limit is 4 s, and --cromarc-user-away (tooling/test-browser).
import {execFileSync} from 'node:child_process';
import path from 'node:path';

const [port] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
const app = process.env.CROMARC_TEST_APP;
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
// Should LaunchServices start a browser of its own instead, it gets a profile
// in this run's folder, never the user's.
const stray = path.join(path.dirname(process.env.CROMARC_TEST_DOWNLOADS), 'stray');
const openLink = url => execFileSync('open', ['-g', '-a', app, url, '--args', `--user-data-dir=${stray}`,
  '--headless=new', '--no-first-run', '--use-mock-keychain', '--disable-features=MacAppCodeSignClone']);
const pageAt = async url => (await targets()).find(t => t.type === 'page' && t.url === url);

const APP = `document.querySelector('cromarc-sidebar-app')`;
const sidebar = await until(async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar')));
const space = await until(() => pageAt('about:blank'));
const url = `${site}/icon.html?little-idle`;
openLink(url);
const little = await until(() => pageAt(url));
const stays = pgrep(`user-data-dir=${stray}`);
if (stays.length) {
  execFileSync('kill', stays);
}
check('setup: the link opened in a Little window', !!little && stays.length === 0);
// The Little window is not used: the space's window comes forward, and the
// run has the user away (--cromarc-user-away), as headless windows do not
// tell which one is in front.
await send(await browserWs(), 'Target.activateTarget', {targetId: space.id});
check('an unused Little window closes by itself',
    !!(await until(async () => !(await pageAt(url)), 20000)),
    JSON.stringify((await targets()).filter(t => t.type === 'page').map(t => t.url)));
const archived = () => evaluate(sidebar,
    `${APP}.proxy_.handler.searchArchive('little-idle', 10).then(({tabs}) => tabs.map(t => [t.url, t.reason]))`);
check('…into the archive, as a Little window\'s page',
    !!(await until(async () => (await archived()).some(([u, reason]) => u === url && reason === 'little'))),
    JSON.stringify(await archived()));
check('…while the space\'s window keeps its page', !!(await pageAt('about:blank')));
process.exit(results.every(Boolean) ? 0 : 1);
