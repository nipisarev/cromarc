// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {texts, textF} from '../strings.mjs';

// Arc import from Arc's export file, when Arc's own data is missing.

const [port = '9343'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
async function send(target, method, params) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => r(JSON.parse(e.data))));
  ws.close();
  return msg;
}
async function evaluate(target, expression) {
  const msg = await send(target, 'Runtime.evaluate',
      {expression, returnByValue: true, awaitPromise: true});
  if (msg.result?.exceptionDetails) throw new Error(JSON.stringify(msg.result.exceptionDetails));
  return msg.result?.result?.value;
}
async function until(fn, ms = 15000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    try { const v = await fn(); if (v) return v; } catch {}
    await sleep(250);
  }
  return null;
}
const importPage = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-import'));
const sidebars = async () => (await targets()).filter(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-arc-import-app')`;
const SIDEBAR_STATE = `(() => {
  const root = document.querySelector('cromarc-sidebar-app').shadowRoot;
  const app = document.querySelector('cromarc-sidebar-app');
  return {
    rows: [...root.querySelectorAll('#tree .row')].map(r => r.querySelector('.title')?.textContent ?? ''),
    today: [...root.querySelectorAll('#today .row .title')].map(e => e.textContent.trim()),
    essentials: [...root.querySelectorAll('.essential')].map(e => e.title),
    spaces: (app.spaces_ ?? []).map(s => s.name),
  };
})()`;

async function openImport() {
  const tab = (await importPage()) ??
      (await targets()).find(t => t.type === 'page' && !t.url.startsWith('chrome://cromarc'));
  await send(tab, 'Page.navigate', {url: 'chrome://cromarc-import'});
  return until(async () => {
    const t = await importPage();
    return t && (await evaluate(t, `${APP} && !${APP}.loading_`)) && t;
  });
}

const page = await openImport();
check('import page loads', !!page);
const T = await texts(evaluate, page, ['arcImportChooseExport', 'arcImportExportHint']);
// The counts as the page and the summary give them: label, then number.
const counted = (key, n) => textF(evaluate, page, key, String(n));
const countsLine = async (...pairs) =>
    (await Promise.all(pairs.map(([key, n]) => counted(key, n)))).join(' · ');
const summaryLine = async (name, folders, pinned, essentials, today, skipped = 0) => {
  const parts = await Promise.all([['arcImportFolders', folders], ['arcImportPinned', pinned],
    ['arcImportEssentials', essentials], ['arcImportToday', today],
    ...(skipped ? [['arcImportSkipped', skipped]] : [])].map(([key, n]) => counted(key, n)));
  return `${name}: ${parts.join(', ')}`;
};

check('no Arc data: the page says so and offers the export file', await evaluate(page,
    `!${APP}.found_ && !${APP}.shadowRoot.querySelector('#missing').hidden && ${APP}.shadowRoot.querySelector('#choose-export').textContent.trim() === ${JSON.stringify(T.arcImportChooseExport)}`));
await evaluate(page, `${APP}.shadowRoot.querySelector('#choose-export').click()`);
const listed = await until(() => evaluate(page, `${APP}.found_ && ({spaces: ${APP}.spaces_.map(s => [s.title, s.folders, s.pinned]),
  counts: ${APP}.shadowRoot.querySelector('.counts').textContent, fromExport: ${APP}.fromExport_,
  hint: [...${APP}.shadowRoot.querySelectorAll('.hint')].filter(h => !h.hidden).map(h => h.textContent.trim().slice(0, 30))})`));
check('the export file lists its space', JSON.stringify(listed?.spaces) === '[["Personal",2,3]]', JSON.stringify(listed));
check('counts without Today/essentials, the export hint shows',
    listed?.counts === await countsLine(['arcImportFolders', 2], ['arcImportPinned', 3]) &&
    listed.hint.length === 1 && T.arcImportExportHint.startsWith(listed.hint[0]), JSON.stringify(listed));
await evaluate(page, `${APP}.shadowRoot.querySelector('#import').click()`);
const summary = await until(() => evaluate(page, `${APP}.summary_`), 20000);
check('imported into the current space',
    summary === `${await summaryLine('Your Chromium', 2, 3, 0, 0)}\n`, summary);
const sidebar = (await sidebars())[0];
const state = await until(async () => {
  const s = await evaluate(sidebar, SIDEBAR_STATE);
  return s.rows.includes('Reading') && s;
});
check('Favorites pinned at the root, folders nested, untitled rows show the host',
    JSON.stringify(state?.rows) === '["127.0.0.1","example.com","Reading","Later","127.0.0.1"]', JSON.stringify(state?.rows));
await evaluate(sidebar, `document.querySelector('cromarc-sidebar-app').shadowRoot.querySelector('#tree .row.pinned').click()`);
const titled = await until(async () => {
  const s = await evaluate(sidebar, SIDEBAR_STATE);
  return s.rows[0] === 'Local Page' && s;
});
check('opened, an untitled item takes its page title', !!titled, JSON.stringify(await evaluate(sidebar, SIDEBAR_STATE)));
const saved = await evaluate(sidebar, `document.querySelector('cromarc-sidebar-app').tree_.map(n => n.title)`);
check('…and keeps it in the tree', saved?.[0] === 'Local Page', JSON.stringify(saved));
console.log(`${results.filter(Boolean).length}/${results.length}`);
process.exit(results.every(Boolean) ? 0 : 1);
