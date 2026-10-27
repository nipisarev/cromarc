// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The language suites: what a person reads, in the words they should read,
// with the browser started in a language the way macOS sets an app's own
// (tooling/test-browser, -AppleLanguages). Cromarc has no language of its own,
// so its words must follow Chromium's.
const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE;
export const phase = Number(process.env.CROMARC_TEST_PHASE ?? 1);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
export const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
export const done = () => process.exit(results.length && results.every(Boolean) ? 0 : 1);
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
const pageOf = prefix => until(async () => (await targets()).find(t => t.url.startsWith(prefix)));
const open = async url => (await send(await browserWs(), 'Target.createTarget', {url})).result.targetId;

const APP = `document.querySelector('cromarc-sidebar-app')`;
const LIB = `${APP}.shadowRoot.querySelector('cromarc-library')`;
const IN = selector => `${LIB}.shadowRoot.querySelector(${JSON.stringify(selector)})`;
const ALL = selector => `[...${LIB}.shadowRoot.querySelectorAll(${JSON.stringify(selector)})]`;
const WELCOME = `document.querySelector('cromarc-welcome-app')`;
const LOAD_TIME_DATA =
    `import('chrome://resources/js/load_time_data.js').then(m => m.loadTimeData)`;
const sidebar = () => pageOf('chrome://cromarc-sidebar');

// Chromium's locale, on our pages and on its own.
export async function checkLocale(expect) {
  const sb = await sidebar();
  const got = await evaluate(sb,
      `(${LOAD_TIME_DATA}).then(l => [l.getString('language'), l.getString('locale')])`);
  check(`Chromium runs in ${expect.locale}, and hands that to Cromarc's pages`,
        got?.[0] === expect.language && got?.[1] === expect.locale, JSON.stringify(got));
  const id = await open('chrome://history');
  const history = await until(async () => (await targets()).find(t => t.id === id && t.url.startsWith('chrome://history')));
  const title = history && await until(() => evaluate(history, `(${LOAD_TIME_DATA}).then(l => l.getString('title'))`));
  check(`…and Chromium's own UI reads in it: its History page is «${expect.history}»`,
        title === expect.history, title);
  await send(await browserWs(), 'Target.closeTarget', {targetId: id});
}

export async function checkSidebar(expect) {
  const sb = await sidebar();
  const newTab = await until(() => evaluate(sb,
      `${APP}.shadowRoot.querySelector('.row.new-tab .title')?.textContent.trim()`));
  check(`the sidebar's new-tab row reads «${expect.newTab}»`, newTab === expect.newTab, newTab);
  const chosen = await evaluate(sb, `(async () => {
    const app = ${APP};
    app.testMenuChoice = ${JSON.stringify(expect.changeName)};
    app.openSpaceMenu_(app.spaces_.find(s => s.current), 10, 10);
    await app.updateComplete;
    return app.testMenuChoice === null;
  })()`);
  const editor = await until(() => evaluate(sb,
      `${APP}.shadowRoot.querySelector('cromarc-space-editor')?.shadowRoot.querySelector('.title')?.textContent.trim()`));
  check(`the space menu has «${expect.changeName}», which opens «${expect.editSpace}»`,
        chosen === true && editor === expect.editSpace, JSON.stringify({chosen, editor}));
  await evaluate(sb, `${APP}.shadowRoot.querySelector('cromarc-space-editor')?.shadowRoot.querySelector('#cancel')?.click(), true`);
}

export async function checkLibrary(expect) {
  const sb = await sidebar();
  // A page visited and a download, so History and Downloads have rows. The
  // download goes to the run's folder, never to ~/Downloads.
  const downloads = process.env.CROMARC_TEST_DOWNLOADS;
  if (!downloads) {
    check('setup: the run has a downloads folder', false);
    done();
  }
  await send(await browserWs(), 'Browser.setDownloadBehavior', {behavior: 'allow', downloadPath: downloads});
  await open(`${site}/a.html`);
  const id = await open(`${site}/download.html`);
  const page = await until(async () => (await targets()).find(t => t.id === id));
  await until(() => evaluate(page, `!!document.getElementById('file')`));
  await evaluate(page, `document.getElementById('file').click(), true`);
  await sleep(2000);
  await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
  await until(() => evaluate(sb, `${APP}.libraryOpen_ && ${APP}.libraryProgress_ === 1 && !!${LIB}`));
  const rail = await until(async () => {
    const labels = await evaluate(sb, `${ALL('.tab .tab-label')}.map(n => n.textContent)`);
    return labels?.length && labels;
  });
  check(`the Library's rail reads ${expect.rail.join(', ')}`, rail?.join() === expect.rail.join(),
        JSON.stringify(rail));
  const H = 'cromarc-library-history';
  const day = await until(() => evaluate(sb, `${IN(`${H} .group h3`)}?.textContent`));
  check(`History's first day reads «${expect.today}»`, day === expect.today, day);
  const D = 'cromarc-library-downloads';
  await evaluate(sb, `${IN('.tab[data-section="downloads"]')}.click(), true`);
  const status = await until(() => evaluate(sb, `${ALL(`${D} .row`)}.find(r =>
      r.querySelector('.row-title').textContent === 'sample.bin')?.querySelector('.download-status').textContent`), 15000);
  check(`a 22-byte download reads «22 ${expect.bytes} — 127.0.0.1 — <time>»`,
        !!status && status.startsWith(`22 ${expect.bytes} — 127.0.0.1 — `), status);
  await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
  await until(() => evaluate(sb, `!${APP}.libraryOpen_`));
}

export async function checkCommandBar(expect) {
  const sb = await sidebar();
  await evaluate(sb, `${APP}.shadowRoot.querySelector('.new-tab').click(), true`);
  const bar = await pageOf('chrome://cromarc-command-bar');
  const placeholder = bar && await until(() => evaluate(bar,
      `document.querySelector('cromarc-command-bar-app')?.shadowRoot.querySelector('#input')?.placeholder`));
  check(`the command bar's field says «${expect.placeholder}»`, placeholder === expect.placeholder, placeholder);
}

async function openWelcome() {
  const id = await open('chrome://cromarc-welcome');
  const page = await until(async () => (await targets()).find(t => t.id === id && t.url.startsWith('chrome://cromarc-welcome')));
  // Its name comes once the page has the space's state.
  await until(() => evaluate(page, `!!${WELCOME}?.shadowRoot.querySelector('#add-work')`));
  await sleep(1000);
  return page;
}

// The welcome page's name for the space, and its other suggestion.
export async function checkWelcome(expect) {
  const page = await openWelcome();
  const got = await evaluate(page, `[${WELCOME}.name_, ${WELCOME}.shadowRoot.querySelector('#add-work').textContent.trim()]`);
  check(`the welcome page calls the space «${expect.spaceName}» and suggests «💼 ${expect.work}»`,
        got?.[0] === expect.spaceName && got?.[1] === `💼 ${expect.work}`, JSON.stringify(got));
  const shown = await evaluate(page, `[...${WELCOME}.shadowRoot.querySelectorAll('section')]
      .filter(s => getComputedStyle(s).display !== 'none').length`);
  check('…showing one step at a time', shown === 1, String(shown));
  return page;
}

// No page of ours shows a key, "undefined" or a $1 where its words should be.
export async function checkNoRawStrings() {
  const pages = (await targets()).filter(t => /^chrome:\/\/cromarc-/.test(t.url));
  const bad = {};
  for (const page of pages) {
    const found = await evaluate(page, `(${LOAD_TIME_DATA}).then(l => {
      const keys = new Set(Object.keys(l.data_ ?? {}));
      const wrong = text => keys.has(text) || /\\bundefined\\b|\\$[1-9]/.test(text);
      const bad = [];
      const walk = root => {
        const texts = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        while (texts.nextNode()) {
          const node = texts.currentNode;
          const text = node.textContent.trim();
          if (text && !['STYLE', 'SCRIPT'].includes(node.parentNode?.nodeName) && wrong(text)) {
            bad.push(text);
          }
        }
        for (const el of root.querySelectorAll('*')) {
          for (const name of ['title', 'aria-label', 'placeholder', 'alt']) {
            const value = el.getAttribute(name);
            if (value !== null && wrong(value.trim())) {
              bad.push(name + '=' + value);
            }
          }
          if (el.shadowRoot) {
            walk(el.shadowRoot);
          }
        }
      };
      walk(document);
      return bad;
    })`);
    if (found?.length) {
      bad[page.url] = found;
    }
  }
  check(`no page of ours shows a raw key, "undefined" or a $1 (${pages.length} pages)`,
        pages.length > 0 && Object.keys(bad).length === 0, JSON.stringify(bad));
}

// Phase 1 of the Russian suite: data made in English that a switch of
// language must leave as it is.
export async function makeData(expect) {
  const sb = await sidebar();
  const welcome = await checkWelcome(expect);
  await evaluate(welcome, `(() => { const a = ${WELCOME}; a.step_ = 'space'; a.onNextClick_(); return true; })()`);
  const named = await until(() => evaluate(sb,
      `${APP}.spaces_.find(s => s.current)?.name === ${JSON.stringify(expect.spaceName)}`));
  check('setup: the space takes the suggested name', !!named);
  await evaluate(sb, `(async () => {
    const app = ${APP};
    app.testMenuChoice = ${JSON.stringify(expect.newFolder)};
    app.shadowRoot.querySelector('#sidebar').dispatchEvent(new MouseEvent('contextmenu',
        {bubbles: true, composed: true, clientX: 50, clientY: 400}));
    await app.updateComplete;
  })()`);
  await until(() => evaluate(sb, `!!${APP}.shadowRoot.querySelector('input.rename')`));
  await evaluate(sb, `${APP}.shadowRoot.querySelector('input.rename').dispatchEvent(new FocusEvent('blur')), true`);
  check('setup: a folder with its default name', !!(await until(() => hasFolder(sb, expect.newFolder))));
  await evaluate(sb, `${APP}.proxy_.handler.addRoute('example.com', 'contains',
      ${APP}.spaces_.find(s => s.current).id), true`);
  check('setup: a routing rule', !!(await until(() => hasRoute(sb))));
}

const hasFolder = (sb, name) => evaluate(sb, `[...${APP}.shadowRoot.querySelectorAll('#tree .row.folder .title')]
    .some(t => t.textContent === ${JSON.stringify(name)})`);
const hasRoute = sb => evaluate(sb, `${APP}.proxy_.handler.getRoutes().then(({routes}) =>
    routes.some(r => r.reference === 'example.com' && r.match === 'contains'))`);

// Phase 2: the same data, in another language.
export async function checkDataKept(expect) {
  const sb = await sidebar();
  const name = await until(() => evaluate(sb, `${APP}.spaces_.find(s => s.current)?.name`));
  check(`after the switch of language the space is still «${expect.spaceName}»`, name === expect.spaceName, name);
  check(`…the folder still «${expect.newFolder}»`, !!(await until(() => hasFolder(sb, expect.newFolder))));
  check('…and the routing rule is there', !!(await until(() => hasRoute(sb))));
}

// Quits the way a person does, for the runner to start the next phase.
export async function restart() {
  const ws = new WebSocket(await browserWs());
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Browser.close'}));
  const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };
  for (let i = 0; i < 60 && alive(); i++) {
    await sleep(500);
  }
  process.exit(alive() ? 1 : 75);
}
