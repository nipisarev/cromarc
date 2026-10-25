// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Space switching, ported from Zen (design, component 4). References are to
// zen-browser/desktop @ f2b18c4f.

// zen:spaces/ZenSpacesSwipe.mjs#L29-L30.
export const SUCCESS_THRESHOLD = 0.25;
export const SUCCESS_VELOCITY_CONTRIBUTION = 0.5;
// zen.workspaces.swipe-actions.delta-multiplier.
export const DELTA_MULTIPLIER = 100;
// zen.workspaces.switch-animation-duration, in seconds.
export const SWITCH_DURATION = 0.25;

// Distance of strip `index` from strip `current` among `count`, the short way
// round (zen:spaces/ZenSpaceManager.mjs, _organizeWorkspaceStripLocations).
export function circularDiff(index: number, current: number, count: number):
    number {
  let diff = index - current;
  const half = Math.floor(count / 2);
  if (diff > half) {
    diff -= count;
  } else if (diff < -half) {
    diff += count;
  }
  return diff;
}

// Where strip `index` stands while `current` is shown and the user's swipe has
// moved things by `offset` (_organizeWorkspaceStripLocations: each strip is
// one width per index away, and every strip follows half the offset).
export function stripTransform(
    index: number, current: number, count: number, offset: number): string {
  return `translateX(${circularDiff(index, current, count) * 100 + offset / 2}%)`;
}

// The space coming in for a swipe offset: a positive offset brings the
// previous one, a negative the next one.
export function neighbourIndex(current: number, count: number, offset: number):
    number {
  if (offset > 0) {
    return (current - 1 + count) % count;
  }
  if (offset < 0) {
    return (current + 1) % count;
  }
  return current;
}

// Whether strip `index` moves when going from `from` to `to`: only the strips
// on the way animate, the others jump (zen #animateTabs, "willBeVisible").
export function isAnimatedStrip(
    index: number, from: number, to: number, count: number): boolean {
  const diff = circularDiff(to, from, count);
  const distance = diff < 0 ? (from - index + count) % count :
                              (index - from + count) % count;
  return distance <= Math.abs(diff);
}

// Turns the swipe tracker's cumulative amount into a strip offset with Zen's
// resistance near the edges (zen:spaces/ZenSpacesSwipe.mjs#L192-L241).
export class SwipeState {
  private lastDelta_ = 0;
  private readonly multiplier_: number;

  constructor(multiplier: number = DELTA_MULTIPLIER) {
    this.multiplier_ = multiplier;
  }

  update(amount: number, stripWidth: number): number {
    const delta = amount * this.multiplier_;
    let translateX = this.lastDelta_ + delta;
    // 4.5 instead of 4 to add a bit of a buffer (Zen's comment).
    const forceMultiplier =
        Math.min(1, 1 - Math.abs(translateX) / (stripWidth * 4.5));
    if (forceMultiplier > 0.5) {
      translateX *= forceMultiplier;
      this.lastDelta_ = delta + (translateX - delta) * 0.5;
    } else {
      translateX = this.lastDelta_;
    }
    return translateX;
  }
}

// Motion's spring with bounce 0 and a duration (Zen animates with
// `{type: "spring", bounce: 0, duration}`): critically damped, with the
// natural frequency that brings it within 0.001 of the target at `duration`,
// found as Motion/popmotion's findSpring() does. Returns position at time t.
export function spring(from: number, to: number, duration: number):
    (t: number) => number {
  const safeMin = 0.001;
  const envelope = (w: number) =>
      -safeMin + Math.exp(-w * duration) * (w * duration + 1);
  const derivative = (w: number) =>
      Math.exp(-w * duration) * (-w * duration * duration);
  let w = 5 / duration;
  for (let i = 0; i < 12; ++i) {
    w = w - envelope(w) / derivative(w);
  }
  const delta = to - from;
  return (t: number) => {
    if (t <= 0) {
      return from;
    }
    return to - Math.exp(-w * t) * (delta + w * delta * t);
  };
}

// The space order after dragging `dragged` onto `target`: it takes the
// target's place. Null when nothing moves.
export function moveSpace(
    ids: string[], dragged: string, target: string): string[]|null {
  const from = ids.indexOf(dragged);
  const to = ids.indexOf(target);
  if (from < 0 || to < 0 || from === to) {
    return null;
  }
  const result = ids.filter(id => id !== dragged);
  result.splice(to, 0, dragged);
  return result;
}

// Zen's add-space swipe (zen:spaces/ZenSpaceAddSwipe.mjs): ready at 80% of
// a swipe, the badge settled by 75% of that, coming 55% of its width in,
// the strips moving 25% aside (halved like every strip offset).
export const ADD_SWIPE_THRESHOLD = 0.8;
export const ADD_SWIPE_ENTRY = 0.75;
export const ADD_SWIPE_TRAVEL = 55;
export const ADD_SWIPE_SHIFT = -25 / 2;

// `amount` is the swipe so far (SwipeTracker: finger travel / 550 px).
export function addSwipeProgress(amount: number): number {
  return Math.min(1.2, Math.abs(amount) / ADD_SWIPE_THRESHOLD);
}

// Zen's #easeInOut for the progress arc.
export function easeInOut(x: number): number {
  const clamped = Math.min(Math.max(x, 0), 1);
  return -(Math.cos(Math.PI * clamped) - 1) / 2;
}

// zen:library/ZenLibrary.mjs, swipeProgress: the Library opens 80% of the
// swipe's share of the strip on top of where it started, rubber-banded past
// shut and fully open.
const LIBRARY_SWIPE_FULL = 0.8;
const LIBRARY_DAMPING_DIMENSION = 0.2;
const LIBRARY_RUBBER_BAND_CONSTANT = 0.08;

function rubberBand(offset: number, dimension: number, constant: number):
    number {
  if (offset === 0 || dimension === 0) {
    return 0;
  }
  return dimension * (1 - Math.exp(-(Math.abs(offset) * constant) / dimension)) *
      Math.sign(offset);
}

export function librarySwipeProgress(
    before: number, offset: number, stripWidth: number): number {
  const progress = before + offset / stripWidth * LIBRARY_SWIPE_FULL;
  if (progress < 0) {
    return rubberBand(
        progress, LIBRARY_DAMPING_DIMENSION, LIBRARY_RUBBER_BAND_CONSTANT);
  }
  if (progress > 1) {
    return 1 +
        rubberBand(progress - 1, LIBRARY_DAMPING_DIMENSION,
                   LIBRARY_RUBBER_BAND_CONSTANT);
  }
  return progress;
}
