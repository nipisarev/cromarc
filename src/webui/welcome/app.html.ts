// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {s} from '../i18n/i18n.js';

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
    <p>${s('welcomeHello')}</p>
    <button class="primary" @click="${this.onNextClick_}">${s('welcomeStart')}</button>
  </section>

  <section ?hidden="${this.step_ !== 'space'}">
    <h1>${s('welcomeFirstSpace')}</h1>
    <div class="name-wrapper">
      <button id="icon" title="${s('welcomeIcon')}" @click="${this.onIconClick_}">
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
    <button class="primary" @click="${this.onNextClick_}">${s('welcomeNext')}</button>
  </section>

  <section ?hidden="${this.step_ !== 'more'}">
    <h1>${s('welcomeMoreTitle')}</h1>
    <p>${s('welcomeMoreBody')}</p>
    <div class="row">
      <button id="add-work" @click="${this.onAddWorkClick_}">💼 ${s('welcomeWork')}</button>
      <input id="own" placeholder="${s('welcomeOwn')}" spellcheck="false"
          @keydown="${this.onOwnKeydown_}">
      <button @click="${this.onAddOwnClick_}">${s('welcomeAdd')}</button>
    </div>
    <ul class="added">
      ${this.added_.map(name => html`<li>${name}</li>`)}
    </ul>
    <button class="primary" @click="${this.onNextClick_}">
      ${this.added_.length ? s('welcomeNext') : s('welcomeLater')}
    </button>
  </section>

  <section ?hidden="${this.step_ !== 'arc'}">
    <h1>${s('welcomeArcTitle')}</h1>
    <p>${s('welcomeArcBody')}</p>
    <button class="primary" @click="${this.onArcClick_}">${s('importButton')}</button>
    <button class="text" @click="${this.onNextClick_}">${s('welcomeSkip')}</button>
  </section>

  <section ?hidden="${this.step_ !== 'default'}">
    <h1>${s('welcomeDefaultTitle')}</h1>
    <p>${s('welcomeDefaultBody')}</p>
    ${this.isDefault_ ? html`<p class="ok">${s('welcomeIsDefault')}</p>` :
      html`<button class="primary" @click="${this.onDefaultClick_}">
        ${s('welcomeMakeDefault')}</button>`}
    <button class="text" @click="${this.onNextClick_}">
      ${this.isDefault_ ? s('welcomeNext') : s('welcomeNotNow')}
    </button>
  </section>

  <section ?hidden="${this.step_ !== 'done'}">
    <h1>${s('welcomeDoneTitle')}</h1>
    <p>${s('welcomeDoneBody')}</p>
    <button class="primary" @click="${this.onFinishClick_}">${s('welcomeFinish')}</button>
  </section>
</main>
<!--_html_template_end_-->`;
  // clang-format on
}
