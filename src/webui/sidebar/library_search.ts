// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// What every searchable section of the Library shares
// (zen:library/sections/ZenLibrarySearchSection.mjs), for sections to call:
// their markup repeats Zen's search top, the lint keeping templates whole.

export interface FilterOption {
  id: string;
  label: string;
  disabled?: boolean;
}

// An `exclusive` group is a radio group whose active option can also be
// turned back off.
export interface FilterGroup {
  id: string;
  title: string;
  exclusive?: boolean;
  options: FilterOption[];
}

export function filterKey(group: string, option: string): string {
  return `${group}:${option}`;
}

// The active filters after the user clicks `optionId` of `group`.
export function toggleFilter(
    active: string[], group: FilterGroup, optionId: string): string[] {
  const key = filterKey(group.id, optionId);
  const wasActive = active.includes(key);
  let next = active;
  if (group.exclusive) {
    next = next.filter(
        other => !group.options.some(o => filterKey(group.id, o.id) === other));
  }
  return wasActive ? next.filter(other => other !== key) : [...next, key];
}

// The results list: `scrolled` once it is, and `onEnd` as its end comes
// within 200px (the sentinel). Returns what stops it.
export function watchResults(section: HTMLElement, onEnd: () => void):
    () => void {
  const results = section.querySelector<HTMLElement>('.search-results');
  const sentinel = section.querySelector<HTMLElement>('.search-sentinel');
  if (!results || !sentinel) {
    return () => {};
  }
  const observer = new IntersectionObserver(entries => {
    if (entries.some(entry => entry.isIntersecting)) {
      onEnd();
    }
  }, {root: results, rootMargin: '200px'});
  observer.observe(sentinel);
  const onScroll = () =>
      results.toggleAttribute('scrolled', results.scrollTop > 0);
  results.addEventListener('scroll', onScroll, {passive: true});
  return () => {
    observer.disconnect();
    results.removeEventListener('scroll', onScroll);
  };
}

// The results slide down by the open filter panel's height.
export function updateFilterHeight(section: HTMLElement) {
  const inner = section.querySelector<HTMLElement>('.filter-panel-inner');
  if (inner) {
    section.style.setProperty(
        '--filter-height', `${inner.scrollHeight + 8}px`);
  }
}
