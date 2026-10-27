// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Cromarc's strings as a page has them. The suites read the labels they look
// for by key, as Chromium's browser tests read theirs with
// l10n_util::GetStringUTF16(IDS_…): a suite passes in any language and fails
// on a wrong key.
const LOAD_TIME_DATA =
    `import('chrome://resources/js/load_time_data.js').then(m => m.loadTimeData)`;

// {key: text} for `keys`, read in `target` through the suite's own evaluate.
export async function texts(evaluate, target, keys) {
  return evaluate(target, `(${LOAD_TIME_DATA}).then(l => Object.fromEntries(
      ${JSON.stringify(keys)}.map(k => [k, l.getString(k)])))`);
}

// The string `key` with its $1… filled, as the page would show it.
export async function textF(evaluate, target, key, ...args) {
  return evaluate(target, `(${LOAD_TIME_DATA}).then(l =>
      l.getStringF(${[key, ...args].map(a => JSON.stringify(a)).join(', ')}))`);
}
