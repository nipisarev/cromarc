// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IconPickerElement} from './icon_picker.js';

export function getHtml(this: IconPickerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header">
  <span class="spacer"></span>
  <div id="page-buttons">
    <button class="page" ?selected="${this.page_ === 'emojis'}"
        @click="${this.onEmojisPageClick_}">Эмодзи</button>
    <button class="page" ?selected="${this.page_ === 'icons'}"
        @click="${this.onIconsPageClick_}">Иконки</button>
  </div>
  <button id="none" title="Без значка" @click="${this.onNoneClick_}">
    <span class="svg" style="mask-image: url(sidebar/ui_icons/trash.svg)"></span>
  </button>
</div>
<div id="pages">
  <div class="pane" id="emojis-pane">
    <div id="search-wrapper">
      <input id="search" type="search" placeholder="Поиск эмодзи"
          spellcheck="false" .value="${this.query_}"
          @input="${this.onSearchInput_}">
    </div>
    <div id="grid" class="list">
      ${this.emojis_().map(emoji => html`
        <button class="cell" data-value="${emoji}"
            @click="${this.onEmojiClick_}">${emoji}</button>`)}
    </div>
  </div>
  <div class="pane list" id="icons">
    ${this.icons_.map(name => html`
      <button class="cell" data-value="${name}" @click="${this.onIconClick_}">
        <span class="svg" style="mask-image: ${this.iconUrl_(name)}"></span>
      </button>`)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
