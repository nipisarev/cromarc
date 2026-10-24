// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The sidebar's background menu opens the Arc import page.
const base = `http://127.0.0.1:${process.argv[2]}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const targets = async () => (await fetch(`${base}/json/list`)).json();
async function evaluate(t, expression) {
  const ws = new WebSocket(t.webSocketDebuggerUrl);
  await new Promise(r => (ws.onopen = r));
  ws.send(JSON.stringify({id: 1, method: 'Runtime.evaluate', params: {expression, returnByValue: true, awaitPromise: true}}));
  const m = await new Promise(r => (ws.onmessage = e => r(JSON.parse(e.data))));
  ws.close(); return m.result?.result?.value;
}
const sidebar = (await targets()).find(t => t.url.startsWith('chrome://cromarc-sidebar'));
const label = await evaluate(sidebar, `(async () => {
  const app = document.querySelector('cromarc-sidebar-app');
  app.testMenuChoice = 'Arc';
  app.onBackgroundContextmenu_(new MouseEvent('contextmenu', {clientX: 20, clientY: 200}));
  await app.updateComplete;
  return app.testMenuChoice === null ? 'Arc' : null;
})()`);
let opened = null;
for (let i = 0; i < 40 && !opened; i++) {
  await sleep(250);
  opened = (await targets()).find(t => t.url.startsWith('chrome://cromarc-import'));
}
console.log(`${label && opened ? 'PASS' : 'FAIL'}: sidebar menu "${label}" opens ${opened?.url}`);
