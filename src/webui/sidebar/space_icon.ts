// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A space icon is an emoji, "icon:<name>" for one of Zen's icons, or the
// name's first letter (which the browser fills in when there is none).
const PREFIX = 'icon:';

export function iconValue(name: string): string {
  return PREFIX + name;
}

// The Zen icon's name, or null for text.
export function svgIconName(icon: string): string|null {
  return icon.startsWith(PREFIX) ? icon.slice(PREFIX.length) : null;
}

// CSS mask for a Zen icon, '' for text icons.
export function iconMask(icon: string): string {
  const name = svgIconName(icon);
  return name ? `url(sidebar/icons/${name}.svg)` : '';
}
