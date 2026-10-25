// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibrarySearchBoxElement} from './library_search_box.js';

export function getHtml(this: LibrarySearchBoxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<span class="svg" style="mask-image: url(sidebar/ui_icons/search.svg)"></span>
<input type="search" placeholder="${this.placeholder}" spellcheck="false"
    @input="${this.onInput_}">
<!--_html_template_end_-->`;
  // clang-format on
}
