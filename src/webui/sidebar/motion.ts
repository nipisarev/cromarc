// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

export interface SpringOptions {
  stiffness: number;
  damping: number;
  mass: number;
}

export interface SpringState {
  value: number;
  done: boolean;
}

// Motion's physical spring (Zen animates with gZenUIManager.motion.animate
// and {type: 'spring', stiffness, damping, mass}), started at rest: the value
// `t` seconds in, and whether it has settled by Motion's rest thresholds for
// values under 5 (rest speed 0.01/s, rest delta 0.005).
export function physicalSpring(
    from: number, to: number, {stiffness, damping, mass}: SpringOptions):
    (t: number) => SpringState {
  const delta = to - from;
  const ratio = damping / (2 * Math.sqrt(stiffness * mass));
  const omega = Math.sqrt(stiffness / mass);
  let position: (t: number) => number;
  if (ratio < 1) {
    const damped = omega * Math.sqrt(1 - ratio * ratio);
    position = t => to -
        Math.exp(-ratio * omega * t) *
            ((ratio * omega * delta / damped) * Math.sin(damped * t) +
             delta * Math.cos(damped * t));
  } else if (ratio === 1) {
    position = t => to - Math.exp(-omega * t) * (delta + omega * delta * t);
  } else {
    const damped = omega * Math.sqrt(ratio * ratio - 1);
    position = t => {
      const envelope = Math.exp(-ratio * omega * t);
      const freq = Math.min(damped * t, 300);
      return to -
          envelope *
          ((ratio * omega * delta) * Math.sinh(freq) +
           damped * delta * Math.cosh(freq)) /
          damped;
    };
  }
  return t => {
    if (t <= 0) {
      return {value: from, done: delta === 0};
    }
    const value = position(t);
    const speed = (value - position(t - 0.005)) / 0.005;
    const done = Math.abs(speed) <= 0.01 && Math.abs(to - value) <= 0.005;
    return {value: done ? to : value, done};
  };
}

// The Library's spring (zen:library/ZenLibrary.mjs, animateProgress).
export const LIBRARY_SPRING: SpringOptions = {
  stiffness: 720,
  damping: 47,
  mass: 1.2,
};

// Motion's spring for {duration, bounce} (motion: generators/spring/find.ts,
// findSpring, from rest): the stiffness whose oscillation has died down to
// 0.001 of its start `duration` seconds in, damped 1 - bounce.
export function springFromDuration(duration: number, bounce: number):
    SpringOptions {
  const safeMin = 0.001;
  const dampingRatio = Math.min(1, Math.max(0.05, 1 - bounce));
  duration = Math.min(10, Math.max(0.01, duration));
  const angular = (frequency: number) =>
      frequency * Math.sqrt(1 - dampingRatio * dampingRatio);
  const envelope = (frequency: number) => {
    const decay = frequency * dampingRatio;
    return safeMin - (decay / angular(frequency)) * Math.exp(-decay * duration);
  };
  const derivative = (frequency: number) => {
    const delta = frequency * dampingRatio * duration;
    const e = dampingRatio ** 2 * frequency ** 2 * duration;
    const factor = -envelope(frequency) + safeMin > 0 ? -1 : 1;
    return (factor * (-e * Math.exp(-delta))) / angular(frequency ** 2);
  };
  let frequency = 5 / duration;
  for (let i = 1; i < 12; i++) {
    frequency -= envelope(frequency) / derivative(frequency);
  }
  if (Number.isNaN(frequency)) {
    // Motion's defaults.
    return {stiffness: 100, damping: 10, mass: 1};
  }
  const stiffness = frequency ** 2;
  return {stiffness, damping: dampingRatio * 2 * Math.sqrt(stiffness), mass: 1};
}

// Zen's theme picker moves its dots with {type: 'spring', duration: 0.4,
// bounce: 0.3} (ZenGradientGenerator.mjs, handleColorPositions).
export const THEME_DOT_SPRING = springFromDuration(0.4, 0.3);

// CSS's cubic-bezier() timing function: progress at time `t`, 0-1.
export function cubicBezier(x1: number, y1: number, x2: number, y2: number):
    (t: number) => number {
  const at = (a: number, b: number, s: number) =>
      3 * a * s * (1 - s) ** 2 + 3 * b * s * s * (1 - s) + s ** 3;
  return t => {
    if (t <= 0 || t >= 1) {
      return Math.min(1, Math.max(0, t));
    }
    // The curve's parameter for this time, by halving.
    let low = 0;
    let high = 1;
    for (let i = 0; i < 30; i++) {
      const mid = (low + high) / 2;
      if (at(x1, x2, mid) < t) {
        low = mid;
      } else {
        high = mid;
      }
    }
    return at(y1, y2, (low + high) / 2);
  };
}

// zen-library.css: the Library's width follows its section over 0.3s.
export const LIBRARY_WIDTH_DURATION = 300;
export const libraryWidthEasing = cubicBezier(.36, .06, 0, .94);
