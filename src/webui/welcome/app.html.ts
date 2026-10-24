// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {WelcomeAppElement} from './app.js';

export function getHtml(this: WelcomeAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<main>
  <div class="dots">
    ${this.steps_.map(step => html`
      <span class="dot" ?current="${step === this.step_}"></span>`)}
  </div>

  <section ?hidden="${this.step_ !== 'hello'}">
    <h1>Cromarc</h1>
    <p>Браузер в духе Arc и Zen, где каждый space — отдельный профиль со своими
      расширениями, паролями и куками.</p>
    <button class="primary" @click="${this.onNextClick_}">Начать</button>
  </section>

  <section ?hidden="${this.step_ !== 'space'}">
    <h1>Это твой первый space</h1>
    <div class="name-wrapper">
      <button id="icon" title="Иконка" @click="${this.onIconClick_}">
        <span class="svg" style="mask-image: ${this.iconMask_()}"
            ?hidden="${!this.iconMask_()}"></span>
        ${this.iconText_()}
      </button>
      <input id="name" .value="${this.name_}" spellcheck="false"
          @input="${this.onNameInput_}">
    </div>
    ${this.pickerOpen_ ? html`
      <cromarc-icon-picker @icon-picked="${this.onIconPicked_}">
      </cromarc-icon-picker>` : nothing}
    <div class="colors">
      ${this.colors_.map(color => html`
        <button class="color" data-color="${color}"
            ?selected="${color === this.color_}"
            style="background: ${this.colorCss_(color)}"
            @click="${this.onColorClick_}"></button>`)}
    </div>
    <button class="primary" @click="${this.onNextClick_}">Дальше</button>
  </section>

  <section ?hidden="${this.step_ !== 'more'}">
    <h1>Добавить ещё?</h1>
    <p>Каждый space — свой профиль: расширения ставятся в него отдельно.
      Например, Bitwarden в личном и 1Password в рабочем.</p>
    <div class="row">
      <button id="add-work" @click="${this.onAddWorkClick_}">💼 Work</button>
      <input id="own" placeholder="Свой…" spellcheck="false"
          @keydown="${this.onOwnKeydown_}">
      <button @click="${this.onAddOwnClick_}">Добавить</button>
    </div>
    <ul class="added">
      ${this.added_.map(name => html`<li>${name}</li>`)}
    </ul>
    <button class="primary" @click="${this.onNextClick_}">
      ${this.added_.length ? 'Дальше' : 'Позже'}
    </button>
  </section>

  <section ?hidden="${this.step_ !== 'arc'}">
    <h1>Перенести из Arc?</h1>
    <p>Нашёлся сайдбар Arc: папки, закреплённые вкладки и essentials можно
      перенести в spaces Cromarc.</p>
    <button class="primary" @click="${this.onArcClick_}">Перенести</button>
    <button class="text" @click="${this.onNextClick_}">Пропустить</button>
  </section>

  <section ?hidden="${this.step_ !== 'default'}">
    <h1>Сделать Cromarc браузером по умолчанию?</h1>
    <p>Тогда ссылки из других приложений открываются в маленьком окне, как
      Little Arc, и правила «ссылка → space» работают.</p>
    ${this.isDefault_ ? html`<p class="ok">Cromarc уже браузер по умолчанию.</p>` :
      html`<button class="primary" @click="${this.onDefaultClick_}">
        Сделать</button>`}
    <button class="text" @click="${this.onNextClick_}">
      ${this.isDefault_ ? 'Дальше' : 'Не сейчас'}
    </button>
  </section>

  <section ?hidden="${this.step_ !== 'done'}">
    <h1>Готово</h1>
    <p>⌘T — командная строка, ⌘S — скрыть сайдбар, Ctrl+1…9 — spaces,
      свайп двумя пальцами по сайдбару — соседний space, ⌥-клик по ссылке —
      Glance.</p>
    <button class="primary" @click="${this.onFinishClick_}">Начать работу</button>
  </section>
</main>
<!--_html_template_end_-->`;
  // clang-format on
}
