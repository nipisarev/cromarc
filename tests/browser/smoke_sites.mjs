// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Real sites in the daily build's configuration (needs the network): each
// one loads in a tab and the browser has to survive it. The runner then lists
// the DCHECKs they tripped in Chromium's code, logged instead of fatal.
const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };

const SITES = [
  'https://www.linkedin.com/',
  'https://www.google.com/',
  'https://mail.google.com/',
  'https://calendar.google.com/',
  'https://docs.google.com/',
  'https://www.youtube.com/',
  'https://github.com/',
  'https://www.reddit.com/',
  'https://x.com/',
  'https://en.wikipedia.org/',
  'https://www.amazon.es/',
  'https://www.notion.so/',
  'https://www.figma.com/',
  'https://slack.com/',
  'https://stackoverflow.com/',
  'https://webauthn.io/',
];

let failed = false;
for (const url of SITES) {
  let tab;
  try {
    tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, {method: 'PUT'})).json();
  } catch {
    tab = null;
  }
  // Long enough for the page and its scripts to run.
  await sleep(8000);
  const ok = alive();
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${url}`);
  if (!ok) {
    failed = true;
    break;
  }
  if (tab?.id) {
    await fetch(`${base}/json/close/${tab.id}`).catch(() => {});
  }
}
process.exit(failed ? 1 : 0);
