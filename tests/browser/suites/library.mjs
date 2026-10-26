// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Library as Zen's (zen:library): its rail, the column it widens, the
// history section with the archive as its closed-tabs source, routes, and
// Zen's edge swipes.
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
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const browserWs = async () => (await (await fetch(`${base}/json/version`)).json()).webSocketDebuggerUrl;
const sb = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const LIB = `${APP}.shadowRoot.querySelector('cromarc-library')`;
const IN = selector => `${LIB}.shadowRoot.querySelector(${JSON.stringify(selector)})`;
const ALL = selector => `[...${LIB}.shadowRoot.querySelectorAll(${JSON.stringify(selector)})]`;
const titles = section => evaluate(sb, `${ALL(`${section} .row .row-title`)}.map(n => n.textContent)`);
const settled = open => until(async () => evaluate(sb,
    `${APP}.libraryOpen_ === ${open} && ${APP}.libraryProgress_ === ${open ? 1 : 0} && !!${LIB} === ${open}`));
const search = (section, text) => evaluate(sb, `(() => {
  const input = ${IN(`${section} .search-box input`)};
  input.value = ${JSON.stringify(text)};
  input.dispatchEvent(new Event('input'));
  return true;
})()`);
const chip = (section, group, option) => evaluate(sb,
    `${IN(`${section} .filter-chip[data-group="${group}"][data-option="${option}"]`)}.click(), true`);

// Two Today tabs, then Clear: they go to the archive.
for (const p of ['a.html', 'b.html']) {
  await send(await browserWs(), 'Target.createTarget', {url: `${site}/${p}`});
}
await until(async () => (await evaluate(sb, `${APP}.tabs_.length`)) >= 3);
await sleep(1500);
await evaluate(sb, `${APP}.proxy_.handler.clearToday()`);
await sleep(800);
const narrow = await evaluate(sb, 'innerWidth');
// The window's height, which the Library takes whole as Zen's does; closed,
// the sidebar sits under Chrome's row of top buttons, above its padding.
const pageTab = (await targets()).find(t => t.type === 'page' && t.url.startsWith(site));
const {windowId} = (await send(await browserWs(), 'Browser.getWindowForTarget', {targetId: pageTab.id})).result;
const windowHeight = (await send(await browserWs(), 'Browser.getWindowBounds', {windowId})).result.bounds.height;
const shortColumn = await evaluate(sb, 'innerHeight');

await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
check('the library button brings the Library out on its spring', !!(await settled(true)));
const wide = await until(async () => { const w = await evaluate(sb, 'innerWidth'); return w >= 418 && w; }, 4000);
check('…and the column widens to Zen\'s 418px', !!wide, `${narrow} → ${await evaluate(sb, 'innerWidth')}`);
const tallColumn = await until(async () => { const h = await evaluate(sb, 'innerHeight'); return h === windowHeight && h; }, 4000);
check('the Library takes the window\'s whole height, under the window buttons', shortColumn < windowHeight && !!tallColumn,
    `${shortColumn} → ${await evaluate(sb, 'innerHeight')} of ${windowHeight}`);
const libraryColumn = await evaluate(sb, `(() => {
  const side = ${IN('#side')}.getBoundingClientRect();
  return {top: side.top, bottom: side.bottom, height: innerHeight,
          tab: ${IN('.tab')}.getBoundingClientRect().top};
})()`);
check('…its column runs from the top edge to the bottom one, the window buttons in its 40px header',
    libraryColumn.top === 0 && libraryColumn.bottom === libraryColumn.height && libraryColumn.tab >= 40,
    JSON.stringify(libraryColumn));
const rail = await evaluate(sb, `${ALL('.tab .tab-label')}.map(n => n.textContent)`);
check('its rail holds Zen\'s sections in Zen\'s order, History chosen', rail.join() === 'Маршруты,Медиа,Загрузки,Пространства,История' &&
    await evaluate(sb, `${IN('.tab[active]')}.dataset.section`) === 'history', JSON.stringify(rail));
check('the rail icons are Zen\'s sprites, coloured', !!(await until(() => evaluate(sb,
    `${IN('.tab[active] .tab-icon-image')}.style.backgroundImage.startsWith('url("data:image/svg+xml;base64')`))));

// History: the pages just visited.
const H = 'cromarc-library-history';
const visited = await until(async () => { const t = await titles(H); return t.includes('Little test') && t; });
check('History lists the pages visited, by day', !!visited &&
    await evaluate(sb, `${IN(`${H} .group h3`)}.textContent`) === 'Сегодня', JSON.stringify(await titles(H)));

// The archive is the closed-tabs source.
await evaluate(sb, `${IN(`${H} .filter-button`)}.click(), true`);
await chip(H, 'source', 'closed');
const archived = await until(async () => { const t = await titles(H); return t.includes('Little test') && t; });
check('the closed-tabs source lists the archive, blank tabs left out', !!archived &&
    !archived.includes('about:blank'), JSON.stringify(await titles(H)));
check('…and the tab reads Archive', await evaluate(sb, `${IN('.tab[active] .tab-label')}.textContent`) === 'Архив');
check('the most visited sort is off for closed tabs', await evaluate(sb,
    `${IN(`${H} .filter-chip[data-option="mostvisited"]`)}.disabled`) === true);
await evaluate(sb, `${IN(`${H} .filter-done`)}.click(), true`);
await search(H, 'little');
const found = await until(async () => { const t = await titles(H); return t.length === 1 && t; });
check('search narrows it', found?.[0] === 'Little test', JSON.stringify(await titles(H)));
await evaluate(sb, `${IN(`${H} .row`)}.click(), true`);
const restored = await until(async () => (await evaluate(sb, `${APP}.tabs_.map(t => t.url)`)).some(u => u.endsWith('a.html')));
check('a click brings the tab back', !!restored);
check('…and shuts the Library, the column back as it was', !!(await settled(false)) &&
    !!(await until(async () => (await evaluate(sb, 'innerWidth')) === narrow, 4000)), String(await evaluate(sb, 'innerWidth')));
check('…the row of top buttons back above the sidebar', !!(await until(async () =>
    (await evaluate(sb, 'innerHeight')) === shortColumn, 4000)), `${await evaluate(sb, 'innerHeight')} ≠ ${shortColumn}`);
const left = await evaluate(sb, `${APP}.proxy_.handler.searchArchive('little', 10).then(({tabs}) => tabs.length)`);
check('…and out of the archive', left === 0, String(left));

// Forgetting a page from history.
await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
await settled(true);
await evaluate(sb, `${IN(`${H} .filter-button`)}.click(), true`);
await chip(H, 'source', 'history');
await evaluate(sb, `${IN(`${H} .filter-done`)}.click(), true`);
await search(H, 'Second');
await until(async () => (await titles(H)).join() === 'Second link');
await evaluate(sb, `${IN(`${H} .row .row-action`)}.click(), true`);
const forgotten = await until(async () => {
  const {visits} = await evaluate(sb, `${APP}.proxy_.handler.queryHistory('Second', 0, 10, 0).then(r => ({visits: r.visits.map(v => v.url)}))`);
  return !visits.some(u => u.endsWith('b.html'));
});
check('the bin forgets the page', !!forgotten && !(await titles(H)).includes('Second link'));

// Routes.
const R = 'cromarc-library-routing';
await evaluate(sb, `${IN('.tab[data-section="space-routing"]')}.click(), true`);
await until(() => evaluate(sb, `!!${IN(R)}`));
check('Routes has Zen\'s empty text', !!(await until(() => evaluate(sb, `${IN(`${R} .empty`)}?.textContent.includes('Маршруты отправляют')`))));
await evaluate(sb, `${IN(`${R} .filter-button`)}.click(), true`);
await evaluate(sb, `(() => { const i = ${IN(`${R} .routing-reference`)}; i.value = 'example.com'; i.dispatchEvent(new Event('input')); return true; })()`);
await sleep(100);
await evaluate(sb, `${IN(`${R} .routing-save`)}.click(), true`);
const route = await until(async () => {
  const {routes} = await evaluate(sb, `${APP}.proxy_.handler.getRoutes().then(r => ({routes: r.routes.map(x => x.reference + '|' + x.match)}))`);
  return routes.includes('example.com|contains') && routes;
});
check('a new route is saved', !!route);
check('…and listed', !!(await until(() => evaluate(sb, `${IN(`${R} .routing-row .routing-value`)}?.textContent === 'example.com'`))));
await evaluate(sb, `${IN(`${R} .routing-row`)}.click(), true`);
await evaluate(sb, `${IN(`${R} .filter-chip[data-match="regex"]`)}.click(), true`);
await sleep(100);
await evaluate(sb, `${IN(`${R} .routing-save`)}.click(), true`);
check('editing keeps one route, changed', !!(await until(async () => {
  const {routes} = await evaluate(sb, `${APP}.proxy_.handler.getRoutes().then(r => ({routes: r.routes.map(x => x.reference + '|' + x.match)}))`);
  return routes.length === 1 && routes[0] === 'example.com|regex';
})));
await evaluate(sb, `${IN(`${R} .routing-row .row-action`)}.click(), true`);
check('the bin removes it', !!(await until(async () => (await evaluate(sb,
    `${APP}.proxy_.handler.getRoutes().then(r => r.routes.length)`)) === 0)));

// Downloads: the runner points them at the profile's folder; DevTools is
// told so too, in case.
const downloadDir = process.env.CROMARC_TEST_DOWNLOADS;
if (!downloadDir) {
  console.log('FAIL: no CROMARC_TEST_DOWNLOADS: downloads would go to ~/Downloads');
  process.exit(1);
}
await send(await browserWs(), 'Browser.setDownloadBehavior', {behavior: 'allow', downloadPath: downloadDir});
const {targetId} = (await send(await browserWs(), 'Target.createTarget', {url: `${site}/download.html`})).result;
const page = await until(async () => (await targets()).find(t => t.id === targetId));
await until(() => evaluate(page, `!!document.getElementById('picture')`));
await evaluate(page, `document.getElementById('file').click(), true`);
await sleep(800);
await evaluate(page, `document.getElementById('picture').click(), true`);
// The new tab shut the Library (Zen's onTabOpen).
check('a tab opening shuts the Library', !!(await settled(false)));
await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
await settled(true);
const D = 'cromarc-library-downloads';
await evaluate(sb, `${IN('.tab[data-section="downloads"]')}.click(), true`);
const files = await until(async () => {
  const t = await titles(D);
  return t.includes('sample.bin') && t.includes('pixel.png') && t;
}, 15000);
const paths = await evaluate(sb, `${APP}.proxy_.handler.getDownloads().then(r => r.downloads.map(d => d.path))`);
check('setup: the files land in the run\'s folder, not ~/Downloads', !!downloadDir &&
    paths.length > 0 && paths.every(p => p.startsWith(downloadDir)), JSON.stringify(paths));
if (!paths.every(p => p.startsWith(downloadDir ?? '\0'))) {
  process.exit(1);
}
check('Downloads lists them, newest first', !!files && files[0] === 'pixel.png', JSON.stringify(await titles(D)));
const status = await evaluate(sb, `${ALL(`${D} .row`)}.find(r => r.querySelector('.row-title').textContent === 'sample.bin').querySelector('.download-status').textContent`);
check('…with Firefox\'s status: size — site — time', /^22 байт — 127\.0\.0\.1 — \d\d:\d\d$/.test(status), status);
const pictureIcon = `${ALL(`${D} .row`)}.find(r => r.querySelector('.row-title').textContent === 'pixel.png').querySelector('.file-icon')`;
check('a finished picture shows itself', !!(await until(() => evaluate(sb,
    `${pictureIcon}.hasAttribute('preview') && ${pictureIcon}.src.includes('download-preview/')`))));
check('…served from the page, the file itself', !!(await until(() => evaluate(sb,
    `${pictureIcon}.complete && ${pictureIcon}.naturalWidth === 1`))), String(await evaluate(sb, `${pictureIcon}.naturalWidth`)));
check('the others get their file icon', (await evaluate(sb, `${ALL(`${D} .row`)}.find(r => r.querySelector('.row-title').textContent === 'sample.bin').querySelector('.file-icon').src`)).startsWith('chrome://fileicon/'));
const typed = async option => { await chip(D, 'type', option); await sleep(200); const t = await titles(D); await chip(D, 'type', option); return t; };
await evaluate(sb, `${IN(`${D} .filter-button`)}.click(), true`);
check('the type filter keeps pictures', JSON.stringify(await typed('images')) === '["pixel.png"]');
await evaluate(sb, `${IN(`${D} .filter-done`)}.click(), true`);
const binId = await evaluate(sb, `${ALL(`${D} .row`)}.find(r => r.querySelector('.row-title').textContent === 'sample.bin').dataset.id`);
// 7: sidebar.mojom DownloadCommand.kDeleteFile.
await evaluate(sb, `${APP}.proxy_.handler.doDownload(${binId}, 7), true`);
check('deleting the file leaves it as gone', !!(await until(async () => (await evaluate(sb,
    `${ALL(`${D} .row[missing] .row-title`)}.map(n => n.textContent)`)).includes('sample.bin'))));
// The download stack on the library button (ZenLibraryWidget).
await evaluate(sb, `${IN('#footer .footer-button')}.click(), true`);
await settled(false);
const button = `${APP}.shadowRoot.querySelector('#library-button')`;
await evaluate(sb, `${button}.dispatchEvent(new MouseEvent('mouseenter')), true`);
check('hovering the library button raises the download stack', !!(await until(() => evaluate(sb,
    `${APP}.stackOpen_ && ${APP}.shadowRoot.querySelector('#foot').hasAttribute('stack-open')`))));
const entries = await evaluate(sb, `[...${APP}.shadowRoot.querySelectorAll('#download-list .download-entry:not([hidden]) .download-entry-title')].map(n => n.textContent)`);
check('…with the last downloads, the newest at the bottom', JSON.stringify(entries) === '["sample.bin","pixel.png"]', JSON.stringify(entries));
check('…and the tabs fading under it', await evaluate(sb, `getComputedStyle(${APP}.shadowRoot.querySelector('#viewport')).maskImage.includes('linear-gradient')`));
await evaluate(sb, `${button}.dispatchEvent(new MouseEvent('mouseleave')), true`);
check('…which goes when the pointer leaves', !!(await until(() => evaluate(sb, `!${APP}.stackOpen_`), 2000)));
await evaluate(sb, `${button}.click()`);
await settled(true);
await evaluate(sb, `${APP}.testMenuChoice = 'Очистить все загрузки'; ${IN('.tab[data-section="downloads"]')}.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, cancelable: true})), true`);
check('the tab\'s menu clears them all', !!(await until(async () => (await titles(D)).length === 0)), JSON.stringify(await titles(D)));
await evaluate(sb, `${IN('.tab[data-section="space-routing"]')}.click(), true`);

// Escape shuts it; the rail remembers the section.
await evaluate(sb, `document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}))`);
check('Escape shuts the Library', !!(await settled(false)));
await evaluate(sb, `${APP}.shadowRoot.querySelector('#library-button').click()`);
await settled(true);
check('…which comes back on the section left', await evaluate(sb, `${IN('.tab[active]')}.dataset.section`) === 'space-routing');
await evaluate(sb, `${IN('#footer .footer-button')}.click(), true`);
check('the back button shuts it', !!(await settled(false)));

// Edge swipes (the only space is both the first and the last).
const swipe = async (amount, direction) => evaluate(sb, `(async () => {
  const app = ${APP};
  app.onSwipe_({type: 0, amount: 0, direction: 0});
  for (let i = 1; i <= 10; i++) app.onSwipe_({type: 1, amount: ${amount} * i / 10, direction: 0});
  await app.updateComplete;
  const state = {drag: app.libraryDrag_, add: app.addProgress_};
  app.onSwipe_({type: 2, amount: ${amount}, direction: ${direction}});
  await app.updateComplete;
  return state;
})()`);
const lib = await swipe(-0.6, -1);
check('swiping back past the first space pulls the Library in', lib.drag > 0, JSON.stringify(lib));
check('…and it opens', !!(await settled(true)));
await swipe(0.6, 1);
check('swiping the other way with it open shuts it (Zen)', !!(await settled(false)));
const add = await swipe(0.9, 1);
check('swiping on past the last space fills the + arc', add.add >= 1, JSON.stringify(add));
check('…and opens the new space form', (await until(() => evaluate(sb, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`))) === true);
await evaluate(sb, `${APP}.onSpaceEditCancel_()`);
const short = await swipe(0.3, 1);
check('a short swipe does not', short.add < 1 && !(await evaluate(sb, `!!${APP}.shadowRoot.querySelector('cromarc-space-editor')`)), JSON.stringify(short));
// In compact mode the column has no width; the Library still takes 418px.
await evaluate(sb, `${APP}.proxy_.handler.toggleCompactMode(), true`);
await until(async () => (await evaluate(sb, 'innerWidth')) < 50, 4000);
await evaluate(sb, `${APP}.openLibrary_(), true`);
check('in compact mode the Library comes out at its width', !!(await settled(true)) &&
    !!(await until(async () => (await evaluate(sb, 'innerWidth')) >= 418, 4000)), String(await evaluate(sb, 'innerWidth')));
await evaluate(sb, `${APP}.closeLibrary_(), true`);
check('…and leaves no column behind', !!(await settled(false)) &&
    !!(await until(async () => (await evaluate(sb, 'innerWidth')) < 50, 4000)), String(await evaluate(sb, 'innerWidth')));
await evaluate(sb, `${APP}.proxy_.handler.toggleCompactMode(), true`);
process.exit(results.every(Boolean) ? 0 : 1);
