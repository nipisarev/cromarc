// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryFiltersElement} from './library_filters.js';

export function getHtml(this: LibraryFiltersElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.groups.map(group => html`
  <div class="filter-group">
    <h3>${group.title}</h3>
    <div class="filter-options">
      ${group.options.map(option => html`
        <button class="filter-chip" data-group="${group.id}"
            data-option="${option.id}"
            ?active="${this.isActive_(group.id, option.id)}"
            ?disabled="${!!option.disabled}" @click="${this.onChipClick_}">
          <span>${option.label}</span>
        </button>`)}
    </div>
  </div>`)}
<!--_html_template_end_-->`;
  // clang-format on
}
