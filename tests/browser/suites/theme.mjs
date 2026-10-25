// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The theme editor: Zen's colour wheel, gradients, grain, saved per space.
// The theme is the window's background, which the browser paints under the
// whole window; it says what it painted in the log (theme runs with
// --cromarc-test-log-background).
import {readFileSync} from 'node:fs';
const [port] = process.argv.slice(2);
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
async function until(fn, ms = 8000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { try { const v = await fn(); if (v) return v; } catch {} await sleep(200); }
  return null;
}
const sb = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const ED = `${APP}.shadowRoot.querySelector('cromarc-theme-editor')`;
const stripBg = () => evaluate(sb, `getComputedStyle(${APP}.shadowRoot.querySelector('.strip')).backgroundImage + ' | ' + getComputedStyle(${APP}.shadowRoot.querySelector('.strip')).backgroundColor`);
// The last window background the browser painted.
const painted = () => (readFileSync(process.env.CROMARC_TEST_LOG, 'utf8').match(/cromarc background: [^\n]*/g) ?? []).at(-1) ?? '';
const paintedSoon = re => until(() => re.test(painted()) && painted());
const colorBefore = await evaluate(sb, `${APP}.spaces_.find(s => s.current).color`);
await evaluate(sb, `(async () => {
  const app = ${APP};
  app.testMenuChoice = 'тему';
  app.openSpaceMenu_(app.spaces_.find(s => s.current), 10, 10);
  await app.updateComplete;
})()`);
check('the space menu opens the theme editor', !!(await until(() => evaluate(sb, `!!${ED}`))));
const tap = (x, y) => evaluate(sb, `(async () => {
  const wheel = ${ED}.shadowRoot.querySelector('#wheel');
  const r = wheel.getBoundingClientRect();
  wheel.dispatchEvent(new PointerEvent('pointerdown', {clientX: r.left + ${x}, clientY: r.top + ${y}, bubbles: true, composed: true, pointerId: 1}));
  wheel.dispatchEvent(new PointerEvent('pointerup', {clientX: r.left + ${x}, clientY: r.top + ${y}, bubbles: true, composed: true, pointerId: 1}));
  await ${ED}.updateComplete;
  return ${ED}.theme.colors.length;
})()`);
check('a click on the wheel picks a colour', (await tap(170, 70)) === 1);
check('…and the window shows it at once, as a preview', !!(await paintedSoon(/colors=1 .*preview=1/)), painted());
check('a second click adds its complement', (await tap(60, 150)) === 2);
check('…two colours, Zen\'s two gradients', !!(await paintedSoon(/colors=2 .*preview=1/)), painted());
const harmony = await evaluate(sb, `(async () => { const e = ${ED}; const before = e.harmony_; e.shadowRoot.querySelector('#harmony').click(); await e.updateComplete; return before + ' -> ' + e.harmony_; })()`);
check('the harmony button cycles', harmony === 'complementary -> singleAnalogous', harmony);
await evaluate(sb, `(() => { const t = ${ED}.shadowRoot.querySelector('#texture'); t.value = '0.6'; t.dispatchEvent(new Event('input')); })()`);
check('grain follows the slider', !!(await paintedSoon(/texture=0\.6 .*preview=1/)), painted());
await evaluate(sb, `${ED}.shadowRoot.querySelector('#done').click()`);
const saved = await until(async () => {
  const s = await evaluate(sb, `${APP}.spaces_.find(s => s.current)`);
  return s.theme && JSON.parse(s.theme).colors.length === 2 && s;
});
check('Готово saves the theme with the space', !!saved, JSON.stringify(await evaluate(sb, `${APP}.spaces_.find(s => s.current)`)));
const colorAfter = await until(async () => {
  const c = await evaluate(sb, `${APP}.spaces_.find(s => s.current).color`);
  return c !== colorBefore && c;
});
check('…and Chrome\'s colour for the profile follows the first colour', !!colorAfter, `${colorBefore} -> ${colorAfter}`);
check('the editor closes and the window keeps the saved theme', !(await evaluate(sb, `!!${ED}`)) &&
    !!(await paintedSoon(/colors=2 texture=0\.6 preview=0/)), painted());
check('…painted by the browser, not the sidebar', (await stripBg()).startsWith('none | rgba(0, 0, 0, 0)'), await stripBg());
process.exit(results.every(Boolean) ? 0 : 1);
