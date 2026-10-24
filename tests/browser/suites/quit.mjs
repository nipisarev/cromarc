// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Quitting the normal way: the browser exits by itself, and the dangling
// raw_ptr check at exit (on in tests) finds nothing; the runner reads the log.
const [port, pid] = process.argv.slice(2);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };

const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const ws = new WebSocket(version.webSocketDebuggerUrl);
await new Promise(r => (ws.onopen = r));
ws.send(JSON.stringify({id: 1, method: 'Browser.close'}));
for (let i = 0; i < 60 && alive(); i++) {
  await sleep(500);
}
const exited = !alive();
console.log(`${exited ? 'PASS' : 'FAIL'}: the browser quits by itself`);
process.exit(exited ? 0 : 1);
