// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Arc import through chrome://cromarc-import with a fixture sidebar.

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
    // Loaded, and rendered.
    return t && (await evaluate(t, `${APP} && !${APP}.loading_ && ${APP}.updateComplete`)) && t;
  });
}

const page = await openImport();
check('import page loads', !!page);
const listed = await evaluate(page, `({found: ${APP}.found_,
  spaces: ${APP}.spaces_.map(s => [s.title, s.folders, s.pinned, s.essentials, s.today]),
  counts: ${APP}.shadowRoot.querySelector('.counts').textContent,
  choices: ${APP}.choices_})`);
check('fixture read: 2 Arc spaces with counts, Today too', listed.found &&
    JSON.stringify(listed.spaces) === JSON.stringify([['Personal', 2, 3, 1, 3], ['Work', 0, 2, 1, 0]]),
    JSON.stringify(listed));
check('counts read in Zen\'s words', listed.counts === '2 папок · 3 закреплённых · 1 важных · 3 в Today', listed.counts);
check('defaults: first to the current space, the other to a new one',
    listed.choices['s-personal'] === 'Default' && listed.choices['s-work'] === 'new',
    JSON.stringify(listed.choices));

await evaluate(page, `${APP}.shadowRoot.querySelector('#import').click()`);
const summary = await until(() => evaluate(page, `${APP}.summary_`), 20000);
console.log('summary:', JSON.stringify(summary));
check('import reports per Cromarc space, Today included, the pinned duplicate left out',
    (summary ?? '').split('\n').filter(Boolean).sort().join('\n') === 'Work: 0 папок, 2 закреплённых, 1 важных, 0 в Today\nYour Chromium: 2 папок, 3 закреплённых, 1 важных, 2 в Today, уже были: 1', summary);

const sidebar = (await sidebars())[0];
const state = await until(async () => {
  const s = await evaluate(sidebar, SIDEBAR_STATE);
  return s.spaces.length === 2 && s.rows.includes('Mail') && s.today.includes('Arc Two') && s;
});
check('current space: Git[Acme App, Acme[Stage]], Mail, essential Calendar',
    state && ['Git', 'Acme App', 'Acme', 'Stage', 'Mail'].every(t => state.rows.includes(t)) &&
    state.essentials.includes('Calendar'), JSON.stringify(state));
check('Today tabs are in Today with Arc\'s titles, Arc\'s top one on top',
    state && state.today.indexOf('Arc One') >= 0 && state.today.indexOf('Arc One') < state.today.indexOf('Arc Two'),
    JSON.stringify(state?.today));
check('a new space "Work" appears in the switcher', state?.spaces.includes('Work'), JSON.stringify(state?.spaces));
await sleep(3000);
const titles = async () => (await evaluate(sidebar, SIDEBAR_STATE)).today.filter(t => /One|Two/.test(t)).sort();
check('the Today tabs did not load (titles still Arc\'s)', JSON.stringify(await titles()) === '["Arc One","Arc Two"]', JSON.stringify(await titles()));
await evaluate(sidebar, `[...document.querySelector('cromarc-sidebar-app').shadowRoot.querySelectorAll('#today .row')].find(r => r.textContent.includes('Arc One')).click()`);
const loaded = await until(async () => (await titles()).includes('Loaded One') && await titles());
check('selected, a Today tab loads', !!loaded, JSON.stringify(await titles()));

// Again: Work now matches by name, nothing is duplicated, no third space.
const again = await openImport();
const choices = await evaluate(again, `${APP}.choices_`);
check('second run maps Work by name', choices['s-work'] !== 'new', JSON.stringify(choices));
await evaluate(again, `${APP}.shadowRoot.querySelector('#import').click()`);
const summary2 = await until(() => evaluate(again, `${APP}.summary_`), 20000);
console.log('summary 2:', JSON.stringify(summary2));
// Spaces answer in the order their profiles load.
const lines = text => (text ?? '').split('\n').filter(Boolean).sort().join('\n');
check('second run adds nothing', lines(summary2) === lines('Your Chromium: 0 папок, 0 закреплённых, 0 важных, 0 в Today, уже были: 7\nWork: 0 папок, 0 закреплённых, 0 важных, 0 в Today, уже были: 3'), summary2);
const state2 = await evaluate((await sidebars())[0], SIDEBAR_STATE);
check('no duplicates, still 2 spaces', state2.rows.filter(t => t === 'Mail').length === 1 &&
    state2.today.filter(t => t.includes('One')).length === 1 && state2.spaces.length === 2, JSON.stringify(state2));
console.log(`${results.filter(Boolean).length}/${results.length}`);
process.exit(results.every(Boolean) ? 0 : 1);
