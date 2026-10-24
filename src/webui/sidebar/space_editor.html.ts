// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpaceEditorElement} from './space_editor.js';

export function getHtml(this: SpaceEditorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="form" @keydown="${this.onFormKeydown_}">
  <div class="header">
    <h1 class="title">${this.creating ? 'Создать пространство' :
                                        'Изменить пространство'}</h1>
    ${this.creating ? html`
      <p class="label">
        Пространства используются для организации ваших вкладок и сеансов.
      </p>` : nothing}
  </div>
  <div class="fields">
    <div class="name-wrapper">
      <button id="icon" title="Изменить значок" @click="${this.onIconClick_}">
        ${this.iconMask_() ? html`
          <span class="svg" style="mask-image: ${this.iconMask_()}"></span>` :
          html`<span class="text">${this.icon}</span>`}
      </button>
      <input id="name" placeholder="Название пространства" spellcheck="false"
          .value="${this.name}" @input="${this.onNameInput_}">
      ${this.pickerOpen ? html`
        <cromarc-icon-picker @icon-picked="${this.onIconPicked_}">
        </cromarc-icon-picker>` : nothing}
    </div>
    ${!this.creating ? html`
      <button id="theme" class="field-button" @click="${this.onThemeClick_}">
        Изменить тему
      </button>` : nothing}
  </div>
  ${this.creating && this.remoteSpaces.length ? html`
    <div id="remote">
      <div class="label">Есть в iCloud Drive — подключить с этого Mac:</div>
      ${this.remoteSpaces.map(space => html`
        <button class="remote-space field-button" data-uuid="${space.uuid}"
            @click="${this.onConnectClick_}">
          <span>${this.remoteIcon_(space)}</span> ${space.name}
        </button>`)}
    </div>` : nothing}
  <div class="buttons">
    <button id="done" class="primary" ?disabled="${!this.name.trim()}"
        @click="${this.onDoneClick_}">
      ${this.creating ? 'Создать пространство' : 'Сохранить'}
    </button>
    <button id="cancel" @click="${this.onCancelClick_}">Отмена</button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
