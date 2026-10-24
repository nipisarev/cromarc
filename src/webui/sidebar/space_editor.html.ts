// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpaceEditorElement} from './space_editor.js';

export function getHtml(this: SpaceEditorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="form" @keydown="${this.onFormKeydown_}">
  <div class="title">${this.creating ? 'Создать space' : 'Изменить space'}</div>
  ${this.creating ? html`
    <p class="label">Spaces упорядочивают вкладки и сессии.</p>` : nothing}
  <div class="name-wrapper">
    <button id="icon" title="Иконка" @click="${this.onIconClick_}">
      <span class="svg" style="mask-image: ${this.iconMask_()}"
          ?hidden="${!this.iconMask_()}"></span>
      <span class="text">${this.iconText_()}</span>
    </button>
    <input id="name" placeholder="Название space" spellcheck="false"
        .value="${this.name}" @input="${this.onNameInput_}">
  </div>
  ${this.pickerOpen ? html`
    <cromarc-icon-picker @icon-picked="${this.onIconPicked_}">
    </cromarc-icon-picker>` : nothing}
  <div class="buttons">
    <button id="done" class="primary" @click="${this.onDoneClick_}">
      ${this.creating ? 'Создать' : 'Сохранить'}
    </button>
    <button id="cancel" @click="${this.onCancelClick_}">Отмена</button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
