// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// node --import this: the pages' imports of Chromium's loadTimeData and of
// /strings.m.js go to the stubs next to it, and their imports of each other
// (as the build serves them, .js) to the .ts sources.
import {existsSync} from 'node:fs';
import {registerHooks} from 'node:module';
import {fileURLToPath} from 'node:url';

const stubs = {
  '//resources/js/load_time_data.js':
      new URL('load_time_data_stub.ts', import.meta.url).href,
  '/strings.m.js': new URL('strings_stub.ts', import.meta.url).href,
};

registerHooks({
  resolve(specifier, context, nextResolve) {
    if (stubs[specifier]) {
      return {url: stubs[specifier], shortCircuit: true};
    }
    if (/^\.\.?\//.test(specifier) && specifier.endsWith('.js') &&
        context.parentURL?.startsWith('file:')) {
      const ts = new URL(specifier.replace(/\.js$/, '.ts'), context.parentURL);
      if (existsSync(fileURLToPath(ts))) {
        return {url: ts.href, shortCircuit: true};
      }
    }
    return nextResolve(specifier, context);
  },
});
