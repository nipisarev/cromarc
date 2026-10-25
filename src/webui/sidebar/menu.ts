// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {BrowserProxy} from './browser_proxy.js';

// An item of a native menu; one with `items` opens a submenu, one with an
// empty label is a separator.
export interface MenuItem {
  label: string;
  action?: () => void;
  checked?: boolean;
  items?: MenuItem[];
}

// For browser tests: the next menu picks the item whose label holds this
// instead of showing.
let testChoice: string|null = null;

export function setTestMenuChoice(choice: string|null) {
  testChoice = choice;
}

export function testMenuChoice(): string|null {
  return testChoice;
}

function flatten(items: MenuItem[]): MenuItem[] {
  return items.flatMap(item => [item, ...(item.items ?? [])]);
}

// Native menus over the window, as Zen's and Arc's: the browser shows the
// labels (sidebar.mojom ShowContextMenu) and answers with the chosen one.
export async function showMenu(x: number, y: number, items: MenuItem[]) {
  const all = flatten(items);
  if (testChoice !== null) {
    const choice = testChoice;
    const item = all.find(item => item.action && item.label.includes(choice));
    if (item) {
      testChoice = null;
      item.action!();
    }
    return;
  }
  const labels = items.flatMap(
      item =>
          [`${item.checked ? '\x01' : ''}${item.label}`,
           ...(item.items ?? [])
               .map(child => `\t${child.checked ? '\x01' : ''}${child.label}`),
  ]);
  const {index} = await BrowserProxy.getInstance().handler.showContextMenu(
      labels, Math.round(x), Math.round(y));
  all[index]?.action?.();
}
