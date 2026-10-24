// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IconPickerElement} from './icon_picker.js';

export function getHtml(this: IconPickerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="bar">
  <input id="search" type="search" placeholder="Поиск" spellcheck="false"
      .value="${this.query_}" @input="${this.onSearchInput_}">
  <button class="page" ?selected="${this.page_ === 'emojis'}"
      title="Эмодзи" @click="${this.onEmojisPageClick_}">🙂</button>
  <button class="page" ?selected="${this.page_ === 'icons'}"
      title="Иконки" @click="${this.onIconsPageClick_}">◇</button>
  <button class="page" title="Без иконки" @click="${this.onNoneClick_}">∅</button>
</div>
<div id="grid" ?hidden="${this.page_ !== 'emojis'}">
  ${this.emojis_().map(emoji => html`
    <button class="cell" data-value="${emoji}"
        @click="${this.onEmojiClick_}">${emoji}</button>`)}
</div>
<div id="icons" ?hidden="${this.page_ !== 'icons'}">
  ${this.icons_.map(name => html`
    <button class="cell" data-value="${name}" title="${name}"
        @click="${this.onIconClick_}">
      <span class="svg" style="mask-image: ${this.iconUrl_(name)}"></span>
    </button>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
