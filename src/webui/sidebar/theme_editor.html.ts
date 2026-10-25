// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeEditorElement} from './theme_editor.js';

export function getHtml(this: ThemeEditorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="editor" @keydown="${this.onKeydown_}">
  <div class="title">Изменить тему</div>
  <div id="wheel" @pointerdown="${this.onWheelPointerdown_}"
      @pointermove="${this.onWheelPointermove_}"
      @pointerup="${this.onWheelPointerup_}">
    ${this.points_.map((point, index) => html`
      <span class="dot" ?primary="${index === 0}" data-index="${index}"
          style="${this.dotStyle_(point, index)}"
          @dblclick="${this.onDotDblclick_}"></span>`)}
  </div>
  <p class="hint">${this.points_.length ?
      'Тяните точки; щелчок — ещё цвет (до 3), двойной щелчок — убрать.' :
      'Щёлкните по кругу, чтобы выбрать цвет.'}</p>
  <button id="harmony" ?hidden="${this.points_.length < 2}"
      @click="${this.onHarmonyClick_}">${this.harmonyLabel_()} ⟳</button>
  <label>Прозрачность
    <input id="opacity" type="range" min="${this.minOpacity_}"
        max="${this.maxOpacity_}" step="0.01" .value="${String(this.theme.opacity)}"
        @input="${this.onOpacityInput_}">
  </label>
  <label>Зерно
    <input id="texture" type="range" min="0" max="1" step="0.05"
        .value="${String(this.theme.texture)}" @input="${this.onTextureInput_}">
  </label>
  <div class="schemes">
    <button data-scheme="auto" ?selected="${this.theme.scheme === 'auto'}"
        @click="${this.onSchemeClick_}">Авто</button>
    <button data-scheme="light" ?selected="${this.theme.scheme === 'light'}"
        @click="${this.onSchemeClick_}">Светлая</button>
    <button data-scheme="dark" ?selected="${this.theme.scheme === 'dark'}"
        @click="${this.onSchemeClick_}">Тёмная</button>
  </div>
  <button id="done" class="primary" @click="${this.onDoneClick_}">Готово</button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
