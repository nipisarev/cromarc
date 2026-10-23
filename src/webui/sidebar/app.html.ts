// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SidebarAppElement} from './app.js';

export function getHtml(this: SidebarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="tabs" role="listbox">
  ${this.tabs_.map(tab => html`
    <div class="tab" role="option" data-id="${tab.id}" title="${tab.title}"
        aria-selected="${tab.isActive}" ?active="${tab.isActive}"
        @click="${this.onTabClick_}">
      ${tab.favicon.dataUrl ?
          html`<img class="favicon" alt="" src="${tab.favicon.dataUrl}">` :
          html`<div class="favicon"></div>`}
      <span class="title">${tab.title || tab.url}</span>
    </div>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
