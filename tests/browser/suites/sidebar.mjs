// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Checks the Cromarc sidebar of a running Chromium over CDP.
import {execSync} from 'node:child_process';

const [port, pid] = process.argv.slice(2);
const windowTitle = () => execSync(`${process.env.CROMARC_TEST_TOOLS}/wintitle ${pid}`).toString().trim();
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function targets() {
  return (await fetch(`${base}/json/list`)).json();
}

function connect(wsUrl) {
  const ws = new WebSocket(wsUrl);
  let id = 0;
  const pending = new Map();
  ws.onmessage = e => {
    const msg = JSON.parse(e.data);
    if (pending.has(msg.id)) {
      pending.get(msg.id)(msg);
      pending.delete(msg.id);
    }
  };
  const ready = new Promise(r => (ws.onopen = r));
  return {
    ready,
    send(method, params = {}) {
      const msgId = ++id;
      ws.send(JSON.stringify({id: msgId, method, params}));
      return new Promise(r => pending.set(msgId, r));
    },
    close: () => ws.close(),
  };
}

async function evaluate(target, expression) {
  const c = connect(target.webSocketDebuggerUrl);
  await c.ready;
  const res = await c.send('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  c.close();
  return res.result?.result?.value ?? res;
}

const titles = ['Cromarc One', 'Cromarc Two', 'Cromarc Three'];
for (const t of titles) {
  await fetch(`${base}/json/new?${encodeURIComponent(`data:text/html,<title>${t}</title>${t}`)}`, {method: 'PUT'});
}
await sleep(1500);

const all = await targets();
const sidebar = all.find(t => t.url.startsWith('chrome://cromarc-sidebar.top-chrome'));
console.log('targets:', all.map(t => `${t.type} ${t.url.slice(0, 60)}`).join('\n  '));
if (!sidebar) {
  console.log('FAIL: no sidebar target');
  process.exit(1);
}

const readSidebar = () => evaluate(sidebar, `(() => {
  const app = document.querySelector('cromarc-sidebar-app');
  const rows = app ? [...app.shadowRoot.querySelectorAll('#sidebar .row.today, #sidebar .row.pinned')] : [];
  return rows.map(r => ({title: r.querySelector('.title').textContent, active: r.hasAttribute('active')}));
})()`);

const before = await readSidebar();
console.log('sidebar rows:', JSON.stringify(before));
const pages = (await targets()).filter(t => t.type === 'page');
console.log('page tabs:', pages.map(p => p.title).join(' | '));

const listed = titles.every(t => before.some(r => r.title === t));
console.log(listed ? 'PASS: all opened tabs are listed' : 'FAIL: opened tabs missing from sidebar');

// Click "Cromarc One" in the sidebar, then ask each page whether it is visible.
await evaluate(sidebar, `(() => {
  const app = document.querySelector('cromarc-sidebar-app');
  const row = [...app.shadowRoot.querySelectorAll('#sidebar .row.today')].find(r => r.querySelector('.title').textContent === 'Cromarc One');
  row.click();
  return true;
})()`);
await sleep(800);
// The window title follows the active tab (read from the window list: a
// page's visibilityState isn't reliable in a test window).
const title = windowTitle();
console.log('window title after click:', title);
const after = await readSidebar();
const activeRow = after.filter(r => r.active).map(r => r.title);
console.log('active row(s) after click:', activeRow.join(' | '));
const activated = title.startsWith('Cromarc One') && activeRow.length === 1 &&
    activeRow[0] === 'Cromarc One';
console.log(activated ? 'PASS: click activates the tab and the sidebar follows' : 'FAIL: activation');

// Close "Cromarc Two" through CDP and check the sidebar drops it.
const two = (await targets()).find(t => t.title === 'Cromarc Two');
await fetch(`${base}/json/close/${two.id}`);
await sleep(800);
const afterClose = await readSidebar();
const dropped = !afterClose.some(r => r.title === 'Cromarc Two');
console.log(dropped ? 'PASS: a closed tab leaves the sidebar' : 'FAIL: closed tab still listed');
process.exit(listed && activated && dropped ? 0 : 1);
