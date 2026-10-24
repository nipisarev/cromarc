// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

export interface ArcSpaceLike {
  arcId: string;
  title: string;
}

export interface TargetLike {
  id: string;
  name: string;
  current: boolean;
}

// Where each Arc space goes before the user changes anything: the Cromarc
// space of the same name; else the first unmatched Arc space goes to the
// current space if nothing took it; the rest become new spaces.
export function defaultTargets(
    spaces: ArcSpaceLike[], targets: TargetLike[],
    newSpace: string): Record<string, string> {
  const choices: Record<string, string> = {};
  const used = new Set<string>();
  for (const space of spaces) {
    const match = targets.find(
        t => t.name.trim().toLowerCase() === space.title.trim().toLowerCase());
    if (match && !used.has(match.id)) {
      choices[space.arcId] = match.id;
      used.add(match.id);
    }
  }
  const current = targets.find(t => t.current);
  for (const space of spaces) {
    if (choices[space.arcId] !== undefined) {
      continue;
    }
    if (current && !used.has(current.id)) {
      choices[space.arcId] = current.id;
      used.add(current.id);
    } else {
      choices[space.arcId] = newSpace;
    }
  }
  return choices;
}
