// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Zen's theme picker: a panel beside the sidebar with the pad, the presets,
// opacity and grain; the theme is saved per space when the panel closes. The
// theme is the window's background, which the browser paints under the
// whole window over Zen's window material; it says what it painted in the
// log (theme runs with --cromarc-test-log-background).
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
const log = () => readFileSync(process.env.CROMARC_TEST_LOG, 'utf8');
// The last window background the browser painted.
const painted = () => (log().match(/cromarc background: [^\n]*/g) ?? []).at(-1) ?? '';
const paintedSoon = re => until(() => re.test(painted()) && painted());

const sb = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const APP = `document.querySelector('cromarc-sidebar-app')`;
const current = () => evaluate(sb, `${APP}.spaces_.find(s => s.current)`);

check('the window has Zen\'s material under it', /cromarc material: installed/.test(log()));

const openFromMenu = () => evaluate(sb, `(async () => {
  const app = ${APP};
  app.testMenuChoice = 'тему';
  app.openSpaceMenu_(app.spaces_.find(s => s.current), 10, 10);
  await app.updateComplete;
})()`);
const pickerTarget = () => until(async () =>
    (await targets()).find(t => t.url.startsWith('chrome://cromarc-theme-picker')));
const P = `document.querySelector('cromarc-theme-picker-app')`;
const visible = t => evaluate(t, `document.visibilityState === 'visible'`);

await openFromMenu();
const picker = await pickerTarget();
check('the space menu opens the theme picker', !!picker && !!(await until(() => visible(picker))));
await until(() => evaluate(picker, `!!${P}?.shadowRoot?.querySelector('#pad')`));
const state = () => evaluate(picker, `(() => {
  const p = ${P};
  return {dots: p.dots_.map(d => ({id: d.id, x: d.position.x, y: d.position.y, type: d.type, color: d.color})),
          harmony: p.harmony_, page: p.page_, theme: p.theme_};
})()`);
const rectOf = selector => evaluate(picker, `(() => {
  const r = ${P}.shadowRoot.querySelector('${selector}').getBoundingClientRect();
  return {x: r.x, y: r.y, w: r.width, h: r.height};
})()`);
// A move with the button held drags; Chromium reads the held button from
// `button` as well as `buttons`.
const mouse = (type, x, y, button = 'left', held = type === 'mousePressed') =>
  send(picker, 'Input.dispatchMouseEvent', {
    type, x, y, button: held || type !== 'mouseMoved' ? button : 'none',
    buttons: held ? (button === 'right' ? 2 : 1) : 0,
    clickCount: type === 'mouseMoved' ? 0 : 1,
  });
const drag = async (from, ...path) => {
  await mouse('mouseMoved', from.x, from.y);
  await mouse('mousePressed', from.x, from.y);
  for (const p of path) {
    await mouse('mouseMoved', p.x, p.y, 'left', true);
  }
  const end = path.at(-1);
  await mouse('mouseReleased', end.x, end.y);
  await sleep(300);
};
const click = async (x, y, button = 'left') => {
  await mouse('mouseMoved', x, y);
  await mouse('mousePressed', x, y, button);
  await mouse('mouseReleased', x, y, button);
  await sleep(150);
};
const clickOn = async selector => {
  const r = await rectOf(selector);
  await click(r.x + r.w / 2, r.y + r.h / 2);
};

check('an empty pad asks for a click', await evaluate(picker,
    `!${P}.shadowRoot.querySelector('#click-to-add').hidden`));
const pad = await rectOf('#pad');
check('the pad is Zen\'s, 360 wide', Math.round(pad.w) === 360 && Math.round(pad.h) === 358, JSON.stringify(pad));
await click(pad.x + 290, pad.y + 110);
let s = await state();
check('a click on the pad places the first colour', s.dots.length === 1 && s.dots[0].type === 'float', JSON.stringify(s.dots));
check('…and the window shows it at once, as a preview', !!(await paintedSoon(/colors=1 .*preview=1/)), painted());

await clickOn('#add');
s = await state();
check('+ adds its complement', s.dots.length === 2 && s.harmony === 'complementary', JSON.stringify(s));
check('…two colours under the window', !!(await paintedSoon(/colors=2 .*preview=1/)), painted());
await clickOn('#toggle');
check('the harmony button cycles', (await state()).harmony === 'singleAnalogous', (await state()).harmony);
await clickOn('#add');
s = await state();
check('+ again makes three, the most Zen allows', s.dots.length === 3 &&
    await evaluate(picker, `${P}.shadowRoot.querySelector('#add').disabled`), JSON.stringify(s.dots));
await clickOn('#remove');
check('− takes the last one away', (await state()).dots.length === 2);

// Dragging the primary dot moves the harmony with it.
s = await state();
const [first, second] = s.dots;
const angleOf = d => Math.atan2(d.y - 179, d.x - 180) * 180 / Math.PI;
await drag({x: pad.x + first.x, y: pad.y + first.y}, {x: pad.x + 120, y: pad.y + 250},
    {x: pad.x + 100, y: pad.y + 260});
const dragged = await state();
const turn = (a, b) => ((a - b) % 360 + 540) % 360 - 180;
check('dragging the first dot moves it and its follower keeps the angle',
    Math.abs(dragged.dots[0].x - 100) < 1 && Math.abs(dragged.dots[0].y - 260) < 1 &&
        Math.abs(turn(angleOf(dragged.dots[1]) - angleOf(dragged.dots[0]),
                      angleOf(second) - angleOf(first))) < 1,
    JSON.stringify(dragged.dots));
check('…and its colour follows', JSON.stringify(dragged.dots[0].color) !== JSON.stringify(first.color));

// The presets: the second page, its first swatch, three analogous dots.
await clickOn('.page-button:last-of-type');
await sleep(800);
check('the arrow turns the preset page', (await state()).page === 1);
await clickOn('.page:nth-child(2) .swatch:nth-child(1)');
s = await state();
check('a three-colour preset gives three analogous dots of its lightness',
    s.dots.length === 3 && s.harmony === 'analogous' &&
        s.dots.every(d => d.type === 'explicit-lightness') && s.theme.lightness === 90,
    JSON.stringify(s));
await clickOn('.page-button:first-of-type');
await sleep(800);
await clickOn('.page:nth-child(1) .swatch:nth-child(4)');
s = await state();
check('a one-colour preset leaves one dot', s.dots.length === 1 && s.theme.lightness === 70, JSON.stringify(s.dots));

// Opacity and grain.
await evaluate(picker, `(() => {
  const o = ${P}.shadowRoot.querySelector('#opacity');
  o.value = '0.65';
  o.dispatchEvent(new Event('input'));
})()`);
check('opacity reaches the window', !!(await paintedSoon(/opacity=0\.65 preview=1/)), painted());
const dial = await rectOf('#texture');
const handle = await rectOf('#texture-handle');
await drag({x: handle.x + handle.w / 2, y: handle.y + handle.h / 2},
    {x: dial.x + dial.w * 0.8, y: dial.y + dial.h * 0.2}, {x: dial.x + dial.w, y: dial.y + dial.h / 2});
check('the grain dial turns to a quarter at 3 o\'clock', (await state()).theme.texture === 0.25,
    String((await state()).theme.texture));
check('…and the grain reaches the window', !!(await paintedSoon(/texture=0\.25 opacity=0\.65 preview=1/)), painted());

// Right-click on the dot removes it.
s = await state();
await click(pad.x + s.dots[0].x, pad.y + s.dots[0].y, 'right');
check('a right-click takes the dot away', (await state()).dots.length === 0);
await click(pad.x + 250, pad.y + 240);
const kept = (await state()).dots;

// Escape closes the panel and saves the theme with the space.
await send(picker, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 53});
check('Escape closes the panel', !!(await until(async () => !(await visible(picker)))));
const saved = await until(async () => {
  const space = await current();
  return space.theme && JSON.parse(space.theme).dots?.length === 1 && space;
});
check('…and the theme is saved with the space, its dot\'s place too', !!saved &&
    Math.abs(JSON.parse(saved.theme).dots[0].x - kept[0].x) < 0.01, JSON.stringify(await current()));
check('…grain and opacity with it', !!saved && JSON.parse(saved.theme).texture === 0.25 &&
    JSON.parse(saved.theme).opacity === 0.65, saved?.theme);
check('the window keeps the saved theme', !!(await paintedSoon(/colors=1 texture=0\.25 opacity=0\.65 preview=0/)), painted());

// Open again: the dot is where it was left.
await openFromMenu();
check('the picker opens again', !!(await until(() => visible(picker))));
const again = await until(async () => { const st = await state(); return st.dots.length === 1 && st; });
check('…with the dot where it was', !!again && Math.abs(again.dots[0].x - kept[0].x) < 0.01 &&
    Math.abs(again.dots[0].y - kept[0].y) < 0.01, JSON.stringify(again?.dots));
await send(picker, 'Input.dispatchKeyEvent', {type: 'rawKeyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27, nativeVirtualKeyCode: 53});
check('closing untouched saves nothing new', !!(await until(async () => !(await visible(picker)))) &&
    (await current()).theme === saved?.theme);
process.exit(results.every(Boolean) ? 0 : 1);
