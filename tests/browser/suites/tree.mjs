// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Exercises the Cromarc tree through the sidebar UI of a running Chromium.

//   phase "build" (default): pin, folder, drag, sleep/wake, away/reset, essentials
//   phase "restart": checks the tree after a relaunch of the same profile
import {execSync} from 'node:child_process';
import {texts} from '../strings.mjs';

const [port = '9334', pid, phase = 'build'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const windowTitle = () => execSync(`${process.env.CROMARC_TEST_TOOLS}/wintitle ${pid}`).toString().trim();

async function targets() {
  return (await fetch(`${base}/json/list`)).json();
}

async function evaluate(target, expression) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Runtime.evaluate',
    params: {expression, returnByValue: true, awaitPromise: true}}));
  const msg = await new Promise(r => (ws.onmessage = e => r(JSON.parse(e.data))));
  ws.close();
  if (msg.result?.exceptionDetails) {
    throw new Error(JSON.stringify(msg.result.exceptionDetails));
  }
  return msg.result?.result?.value;
}

async function navigate(target, url) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Page.navigate', params: {url}}));
  await new Promise(r => (ws.onmessage = r));
  ws.close();
}

const page = title => `data:text/html,<title>${title}</title>${title}`;
const sidebarTarget = async () => (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));

// Helpers injected into the sidebar page.
const HELPERS = `
  const app = document.querySelector('cromarc-sidebar-app');
  const root = app.shadowRoot;
  const rowByTitle = (sel, title) => [...root.querySelectorAll(sel)]
      .find(r => (r.querySelector('.title')?.textContent ?? r.title) === title);
  const state = () => ({
    rows: [...root.querySelectorAll('#tree .row')].map(r => ({
      title: r.querySelector('.title')?.textContent ?? '',
      kind: r.classList.contains('folder') ? 'folder' : 'pinned',
      state: r.getAttribute('state'), depth: Number(r.style.getPropertyValue('--depth')),
      active: r.hasAttribute('active')})),
    today: [...root.querySelectorAll('#today .row')].map(r => r.querySelector('.title').textContent),
    essentials: [...root.querySelectorAll('.essential')].map(e => e.title),
  });
  const menu = async (el, label) => {
    const r = el.getBoundingClientRect();
    app.testMenuChoice = label;
    el.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true,
        clientX: r.left + 5, clientY: r.top + 5}));
    await app.updateComplete;
    if (app.testMenuChoice !== null) throw new Error('no menu item ' + label);
  };
`;

async function inSidebar(body) {
  const sb = await sidebarTarget();
  return evaluate(sb, `(async () => { ${HELPERS} ${body} })()`);
}

if (phase === 'build') {
  for (const t of ['Cromarc A', 'Cromarc B']) {
    await fetch(`${base}/json/new?${encodeURIComponent(page(t))}`, {method: 'PUT'});
  }
  await sleep(1500);

  const T = await texts(evaluate, await sidebarTarget(),
      ['pin', 'newFolder', 'backToPinned', 'separateFromPinned', 'addToEssentials']);
  // Pin A from Today.
  await inSidebar(`await menu(rowByTitle('#today .row', 'Cromarc A'), ${JSON.stringify(T.pin)});`);
  await sleep(800);
  let s = await inSidebar('return state();');
  check('pin: A leaves Today and appears in the tree, live',
      !s.today.includes('Cromarc A') &&
      s.rows.some(r => r.title === 'Cromarc A' && r.state === 'live'), JSON.stringify(s));

  // New folder at the root, renamed to Git.
  await inSidebar(`
    const bg = root.querySelector('#sidebar');
    app.testMenuChoice = ${JSON.stringify(T.newFolder)};
    bg.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, composed: true, clientX: 50, clientY: 400}));
    await app.updateComplete;
  `);
  await sleep(800);
  await inSidebar(`
    const input = root.querySelector('input.rename');
    input.value = 'Git';
    input.dispatchEvent(new FocusEvent('blur'));
  `);
  await sleep(800);
  s = await inSidebar('return state();');
  check('folder: created and renamed', s.rows.some(r => r.kind === 'folder' && r.title === 'Git'),
      JSON.stringify(s));

  // Drag A into Git (drop in the middle of the folder row = inside).
  await inSidebar(`
    const a = rowByTitle('#tree .row', 'Cromarc A');
    const git = rowByTitle('#tree .row', 'Git');
    const dt = new DataTransfer();
    a.dispatchEvent(new DragEvent('dragstart', {bubbles: true, composed: true, dataTransfer: dt}));
    const r = git.getBoundingClientRect();
    const at = {bubbles: true, composed: true, cancelable: true, dataTransfer: dt,
        clientX: r.left + 20, clientY: r.top + r.height / 2};
    git.dispatchEvent(new DragEvent('dragover', at));
    git.dispatchEvent(new DragEvent('drop', at));
    a.dispatchEvent(new DragEvent('dragend', {bubbles: true, composed: true}));
  `);
  await sleep(800);
  s = await inSidebar('return state();');
  const a = s.rows.find(r => r.title === 'Cromarc A');
  check('drag: A moves inside Git', a && a.depth === 1, JSON.stringify(s));

  // Sleep: close A's live tab with ×.
  await inSidebar(`rowByTitle('#tree .row', 'Cromarc A').querySelector('.close').click();`);
  await sleep(800);
  s = await inSidebar('return state();');
  check('sleep: closing the live tab leaves A asleep in the tree',
      s.rows.some(r => r.title === 'Cromarc A' && r.state === 'asleep'), JSON.stringify(s));

  // Wake: click A, its home URL opens and becomes the window's tab.
  await inSidebar(`rowByTitle('#tree .row', 'Cromarc A').click();`);
  await sleep(1200);
  s = await inSidebar('return state();');
  check('wake: clicking A opens its home URL, live and active',
      s.rows.some(r => r.title === 'Cromarc A' && r.state === 'live' && r.active) &&
      windowTitle().startsWith('Cromarc A'), JSON.stringify(s) + ' title=' + windowTitle());

  // Away: navigate A's tab elsewhere, then ↩ brings it home.
  const aTab = (await targets()).find(t => t.type === 'page' && t.title === 'Cromarc A');
  await navigate(aTab, page('Elsewhere'));
  await sleep(1200);
  s = await inSidebar('return state();');
  check('away: A shows the away state', s.rows.some(r => r.title === 'Cromarc A' && r.state === 'away'),
      JSON.stringify(s));
  // As Zen: the icon is the reset button, its sublabel says where it goes.
  const sub = await inSidebar(`const row = rowByTitle('#tree .row', 'Cromarc A');
    const b = row.querySelector('.reset-pin');
    const before = row.querySelector('.sublabel')?.textContent;
    b.dispatchEvent(new MouseEvent('mousemove', {metaKey: true, bubbles: true}));
    await new Promise(r => setTimeout(r, 100));
    const cmd = row.querySelector('.sublabel')?.textContent;
    b.dispatchEvent(new MouseEvent('mousemove', {metaKey: false, bubbles: true}));
    return {button: !!b, old: !!row.querySelector('.reset'), before, cmd};`);
  check('away: the icon is a reset button, no ↩', sub.button && !sub.old, JSON.stringify(sub));
  check('away: sublabels are Zen\'s', sub.before === T.backToPinned &&
      sub.cmd === T.separateFromPinned, JSON.stringify(sub));
  await inSidebar(`rowByTitle('#tree .row', 'Cromarc A').querySelector('.reset-pin').click();`);
  await sleep(1200);
  s = await inSidebar('return state();');
  check('reset: the icon returns A to its home URL', s.rows.some(r => r.title === 'Cromarc A' && r.state === 'live'),
      JSON.stringify(s));
  const homeTab = (await targets()).find(t => t.type === 'page' && t.title === 'Cromarc A');
  check('reset: only the home page is left in its history (Zen)',
      (await evaluate(homeTab, 'history.length')) === 1, String(await evaluate(homeTab, 'history.length')));

  // Cmd+click: the page moves to an ordinary tab, A resets.
  await navigate(homeTab, page('Elsewhere'));
  await sleep(1200);
  await inSidebar(`rowByTitle('#tree .row', 'Cromarc A').querySelector('.reset-pin')
      .dispatchEvent(new MouseEvent('click', {metaKey: true, bubbles: true}));`);
  await sleep(1500);
  s = await inSidebar('return state();');
  const pages = (await targets()).filter(t => t.type === 'page');
  const moved = pages.find(t => t.title === 'Elsewhere');
  check('separate: A is back home and live', s.rows.some(r => r.title === 'Cromarc A' && r.state === 'live'),
      JSON.stringify(s));
  check('separate: the page went on in an ordinary tab with its history',
      !!moved && (await evaluate(moved, 'history.length')) >= 2 &&
      s.today.includes('Elsewhere'), JSON.stringify({pages: pages.map(t => t.title), today: s.today}));

  // Essentials: pin B straight into the grid.
  await inSidebar(`await menu(rowByTitle('#today .row', 'Cromarc B'), ${JSON.stringify(T.addToEssentials)});`);
  await sleep(800);
  s = await inSidebar('return state();');
  check('essentials: B becomes a tile and leaves Today',
      s.essentials.includes('Cromarc B') && !s.today.includes('Cromarc B'), JSON.stringify(s));
} else {
  await sleep(1500);
  const s = await inSidebar('return state();');
  check('restart: Git with A inside survives, asleep',
      s.rows.some(r => r.kind === 'folder' && r.title === 'Git') &&
      s.rows.some(r => r.title === 'Cromarc A' && r.depth === 1), JSON.stringify(s));
  check('restart: B is still an essential', s.essentials.includes('Cromarc B'), JSON.stringify(s));
}

process.exit(results.every(Boolean) ? 0 : 1);
