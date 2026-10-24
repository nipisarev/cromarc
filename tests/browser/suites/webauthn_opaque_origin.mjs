// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Runs as the daily build does (debug checks not fatal). A sandboxed iframe,
// whose origin is opaque, asks WebAuthn for a platform authenticator: this
// trips a DCHECK in Chromium's own code (authenticator_common_impl.cc) and,
// with DCHECKs fatal, took the whole browser down on LinkedIn (24.10.2026).
// The daily build logs it and goes on.
import {readFileSync} from 'node:fs';

const [port, pid] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const site = process.env.CROMARC_TEST_SITE ?? 'http://127.0.0.1:9599';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
const check = (name, ok, detail = '') => {
  results.push(ok);
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok ? '' : '  ' + detail}`);
};
const alive = () => { try { process.kill(Number(pid), 0); return true; } catch { return false; } };
const pageTitle = async () => {
  try {
    const list = await (await fetch(`${base}/json/list`)).json();
    return list.find(t => t.url === `${site}/uvpaa.html`)?.title ?? '';
  } catch {
    return '';
  }
};

await fetch(`${base}/json/new?${encodeURIComponent(`${site}/uvpaa.html`)}`, {method: 'PUT'});
let title = '';
for (let i = 0; i < 40 && !/^(uvpaa|err):/.test(title); i++) {
  await sleep(250);
  title = await pageTitle();
}
await sleep(1000);
check('the browser survives WebAuthn from an opaque origin', alive());
check('the page gets its answer', /^(uvpaa|err):/.test(title), title);
const log = process.env.CROMARC_TEST_LOG ? readFileSync(process.env.CROMARC_TEST_LOG, 'utf8') : '';
check('the DCHECK is logged, not fatal', /DCHECK failed: !caller_origin\.opaque\(\)/.test(log) && !/FATAL/.test(log));
process.exit(results.every(Boolean) ? 0 : 1);
