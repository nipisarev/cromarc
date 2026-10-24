// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CommandBarAppElement} from './app.js';

export function getHtml(this: CommandBarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="card">
  <input id="input" .value="${this.text_}" spellcheck="false"
      autocomplete="off" placeholder="Поиск или адрес"
      @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}">
  <div id="results" role="listbox" ?hidden="${this.results_.length === 0}">
    ${this.results_.map((result, index) => html`
      <div class="result" role="option" data-index="${index}"
          ?selected="${this.isSelected_(index)}"
          @click="${this.onResultClick_}"
          @mouseenter="${this.onResultMouseenter_}">
        <span class="icon"
            style="background-image: ${this.iconFor_(result)}"></span>
        <span class="text">
          <span class="title">${result.title || result.subtitle}</span>
          <span class="subtitle">${result.subtitle}</span>
        </span>
        <span class="kind">${this.labelFor_(result)}</span>
      </div>`)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
