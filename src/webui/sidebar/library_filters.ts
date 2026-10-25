// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './library_filters.html.js';
import {filterKey} from './library_search.js';
import type {FilterGroup} from './library_search.js';

export interface FilterToggle {
  group: FilterGroup;
  option: string;
}

// Zen's filter panel groups (ZenLibrarySearchSection #renderFilterPanel):
// fires `filter-toggled` with the group and the option clicked.
export class LibraryFiltersElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-filters';
  }

  override createRenderRoot() {
    return this;
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      groups: {type: Array},
      active: {type: Array},
    };
  }

  accessor groups: FilterGroup[] = [];
  accessor active: string[] = [];

  protected isActive_(group: string, option: string): boolean {
    return this.active.includes(filterKey(group, option));
  }

  protected onChipClick_(e: Event) {
    const {group, option} = (e.currentTarget as HTMLElement).dataset;
    const found = this.groups.find(g => g.id === group);
    if (found && option) {
      this.fire('filter-toggled', {group: found, option});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-filters': LibraryFiltersElement;
  }
}

customElements.define(LibraryFiltersElement.is, LibraryFiltersElement);
