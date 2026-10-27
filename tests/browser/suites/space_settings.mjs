// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Space settings: create (Zen's form, a Zen icon), edit, reorder, sleep, delete.

import {execSync} from 'node:child_process';
import {texts} from '../strings.mjs';
const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const targets = async () => (await fetch(`${base}/json/list`)).json();
let nextId = 1;
async function send(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl ?? target);
  await new Promise(r => (ws.onopen = r));
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id === id) r(m); }));
  ws.close(); return msg;
}
const evaluate = async (t, expression) => {
  const m = await send(t, 'Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  if (m.result?.exceptionDetails) throw new Error(JSON.stringify(m.result.exceptionDetails).slice(0, 300));
  return m.result?.result?.value;
};
async function until(fn, ms = 10000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const sidebars = async () => (await targets()).filter(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const spacesOf = t => evaluate(t, `${APP}.spaces_.map(s => ({id: s.id, name: s.name, icon: s.icon, current: s.current}))`);
// The sidebar whose own space is `name`.
const sidebarOf = async name => {
  for (const t of await sidebars()) {
    const spaces = await spacesOf(t).catch(() => []);
    if (spaces.find(s => s.current)?.name === name) return t;
  }
  return null;
};
const visibleTitles = () => execSync(`${process.env.CROMARC_TEST_TOOLS}/onscreen ${pid}`).toString().trim().split('\n').filter(Boolean).length;

const first = (await sidebars())[0];
const firstName = (await spacesOf(first)).find(s => s.current).name;
console.log('first space:', firstName);

// Create through the form, with an icon from Zen's set.
// Zen's "+" menu (a native menu, chosen through testMenuChoice).
const {newSpace} = await texts(evaluate, first, ['newSpace']);
await evaluate(first, `${APP}.testMenuChoice = ${JSON.stringify(newSpace)}; ${APP}.shadowRoot.querySelector('#create-new').click()`);
const formOpen = await until(() => evaluate(first, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`));
check('"+" opens the space form', !!formOpen);
await evaluate(first, `(async () => {
  const editor = ${APP}.shadowRoot.querySelector('cromarc-space-editor');
  const input = editor.shadowRoot.querySelector('#name');
  input.value = 'Work'; input.dispatchEvent(new Event('input'));
  editor.shadowRoot.querySelector('#icon').click();
  await editor.updateComplete;
  const picker = editor.shadowRoot.querySelector('cromarc-icon-picker');
  picker.shadowRoot.querySelectorAll('.page')[1].click();
  await picker.updateComplete;
  picker.shadowRoot.querySelector('#icons [data-value="briefcase"]').click();
  await editor.updateComplete;
  editor.shadowRoot.querySelector('#done').click();
})()`);
// The icon lands once the new profile has loaded, a moment after the name.
const created = await until(async () => (await spacesOf(first)).find(s => s.name === 'Work' && s.icon === 'icon:briefcase')) ??
    (await spacesOf(first)).find(s => s.name === 'Work');
check('the new space is listed with its Zen icon', created?.icon === 'icon:briefcase', JSON.stringify(created));
const work = await until(() => sidebarOf('Work'), 15000);
check('…and has its own window and sidebar', !!work);
const iconShown = work && await until(() => evaluate(work, `getComputedStyle(${APP}.shadowRoot.querySelector('.space-button[current] .svg')).maskImage.includes('briefcase.svg')`));
check('…the switcher shows the icon', !!iconShown);
const svgOk = work && await evaluate(work, `new Promise(r => { const i = new Image(); i.onload = () => r(i.naturalWidth); i.onerror = () => r('error'); i.src = 'sidebar/icons/briefcase.svg'; })`);
check('…and the icon file loads', typeof svgOk === 'number' && svgOk > 0, String(svgOk));

// Edit: name and icon.
await evaluate(work, `${APP}.onSpaceNameDblclick_()`);
await until(() => evaluate(work, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`));
await evaluate(work, `(async () => {
  const editor = ${APP}.shadowRoot.querySelector('cromarc-space-editor');
  const input = editor.shadowRoot.querySelector('#name');
  input.value = 'Job'; input.dispatchEvent(new Event('input'));
  editor.shadowRoot.querySelector('#icon').click();
  await editor.updateComplete;
  const picker = editor.shadowRoot.querySelector('cromarc-icon-picker');
  const search = picker.shadowRoot.querySelector('#search');
  search.value = 'rocket'; search.dispatchEvent(new Event('input'));
  await picker.updateComplete;
  picker.shadowRoot.querySelector('#grid .cell').click();
  await editor.updateComplete;
  editor.shadowRoot.querySelector('#done').click();
})()`);
const edited = await until(async () => (await spacesOf(work)).find(s => s.name === 'Job' && s.icon && !s.icon.startsWith('icon:') && s.icon !== 'J'));
check('edit: renamed and an emoji found by tag', !!edited, JSON.stringify(await spacesOf(work)));

// Reorder.
const before = (await spacesOf(work)).map(s => s.id);
await evaluate(work, `${APP}.proxy_.handler.reorderSpaces(${JSON.stringify([...before].reverse())})`);
const reordered = await until(async () => JSON.stringify((await spacesOf(work)).map(s => s.id)) === JSON.stringify([...before].reverse()));
check('reorder', !!reordered);

// Sleep the first space: its window keeps one new tab.
const firstSb = await sidebarOf(firstName);
const firstId = (await spacesOf(work)).find(s => s.name === firstName).id;
await evaluate(work, `${APP}.proxy_.handler.sleepSpace(${JSON.stringify(firstId)})`);
const slept = await until(async () => {
  const tabs = await evaluate(firstSb, `${APP}.tabs_.map(t => t.url)`);
  return tabs.length === 1 && tabs[0].includes('chrome://newtab') && tabs;
});
check('sleep: the space keeps one new tab', !!slept, JSON.stringify(await evaluate(firstSb, `${APP}.tabs_.map(t => t.url)`)));

// Delete Job from its own window: back to the first space, one space left.
const jobId = edited.id;
await evaluate(work, `${APP}.deleting_ = ${APP}.spaces_.find(s => s.id === ${JSON.stringify(jobId)})`);
await sleep(200);
const confirmShown = await evaluate(work, `!!${APP}.shadowRoot.querySelector('#delete-space')`);
check('delete asks first', confirmShown);
await evaluate(work, `${APP}.shadowRoot.querySelector('#delete-space .danger').click()`);
const gone = await until(async () => {
  const sb = (await sidebars())[0];
  return sb && (await spacesOf(sb)).length === 1 && (await sidebars()).length === 1;
}, 20000);
check('delete: the space and its window are gone', !!gone, `${(await sidebars()).length} sidebars`);
process.exit(results.every(Boolean) ? 0 : 1);
