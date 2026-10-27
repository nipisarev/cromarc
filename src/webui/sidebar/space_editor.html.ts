// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {s} from '../i18n/i18n.js';

import type {SpaceEditorElement} from './space_editor.js';

export function getHtml(this: SpaceEditorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="form" @keydown="${this.onFormKeydown_}">
  <div class="header">
    <h1 class="title">${this.creating ? s('createSpaceTitle') :
                                        s('editSpaceTitle')}</h1>
    ${this.creating ? html`
      <p class="label">${s('createSpaceLabel')}</p>` : nothing}
  </div>
  <div class="fields">
    <div class="name-wrapper">
      <button id="icon" title="${s('changeIcon')}" @click="${this.onIconClick_}">
        ${this.iconMask_() ? html`
          <span class="svg" style="mask-image: ${this.iconMask_()}"></span>` :
          html`<span class="text">${this.icon}</span>`}
      </button>
      <input id="name" placeholder="${s('spaceName')}" spellcheck="false"
          .value="${this.name}" @input="${this.onNameInput_}">
      ${this.pickerOpen ? html`
        <cromarc-icon-picker @icon-picked="${this.onIconPicked_}">
        </cromarc-icon-picker>` : nothing}
    </div>
    ${!this.creating ? html`
      <button id="theme" class="field-button" @click="${this.onThemeClick_}">
        ${s('editTheme')}
      </button>` : nothing}
  </div>
  ${this.creating && this.remoteSpaces.length ? html`
    <div id="remote">
      <div class="label">${s('remoteSpaces')}</div>
      ${this.remoteSpaces.map(space => html`
        <button class="remote-space field-button" data-uuid="${space.uuid}"
            @click="${this.onConnectClick_}">
          <span>${this.remoteIcon_(space)}</span> ${space.name}
        </button>`)}
    </div>` : nothing}
  <div class="buttons">
    <button id="done" class="primary" ?disabled="${!this.name.trim()}"
        @click="${this.onDoneClick_}">
      ${this.creating ? s('newSpace') : s('save')}
    </button>
    <button id="cancel" @click="${this.onCancelClick_}">${s('cancel')}</button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
