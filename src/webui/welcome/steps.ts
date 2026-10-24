// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The first run's steps (design, UI section 12); Arc's only when Arc is on
// this Mac.
export type Step = 'hello'|'space'|'more'|'arc'|'default'|'done';

export function stepsFor(arcFound: boolean): Step[] {
  return arcFound ? ['hello', 'space', 'more', 'arc', 'default', 'done'] :
                    ['hello', 'space', 'more', 'default', 'done'];
}

export function nextStep(steps: Step[], current: Step): Step {
  const index = steps.indexOf(current);
  return steps[Math.min(index + 1, steps.length - 1)]!;
}

// Space colours to start with, as 0xAARRGGBB.
export const COLORS: readonly number[] = [
  0xFF6B40D9, 0xFF3B82F6, 0xFF1F9E8F, 0xFF5CA83D,
  0xFFE0B12E, 0xFFE07A2E, 0xFFD9407A, 0xFF6B7280,
];

export function cssColor(argb: number): string {
  return `#${(argb & 0xFFFFFF).toString(16).padStart(6, '0')}`;
}
