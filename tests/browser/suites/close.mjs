// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Opens the command bar, then closes the window by closing its only tab, as
// Cmd+W does.
const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const targets = async () => (await fetch(`${base}/json/list`)).json();
async function cdp(target, method, params = {}) {
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method, params}));
  const msg = await new Promise(r => (ws.onmessage = e => r(JSON.parse(e.data))));
  ws.close();
  return msg.result;
}
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };

const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
await cdp(sidebar, 'Runtime.evaluate', {expression:
  `document.querySelector('cromarc-sidebar-app').shadowRoot.querySelector('.new-tab').click()`});
await sleep(1500);
const bar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-command-bar'));
console.log(`${bar ? 'PASS' : 'FAIL'}: the command bar opened`);
// Escape hides it, as a user would before closing.
for (const tab of (await targets()).filter(t => t.type === 'page' && !t.url.startsWith('chrome://'))) {
  await fetch(`${base}/json/close/${tab.id}`);
}
await sleep(3000);
console.log(`${alive() ? 'PASS' : 'FAIL'}: the browser survives closing the window`);
