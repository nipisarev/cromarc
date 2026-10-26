// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {textureDotLit} from '../sidebar/theme_model.js';

import type {ThemePickerAppElement} from './app.js';
import {swatchStyle} from './presets.js';

export function getHtml(this: ThemePickerAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="panel" ?invalidate-controls="${this.isDefault_()}">
  <div id="pad" @click="${this.onPadClick_}">
    <div id="scheme" class="buttons">
      ${(['auto', 'light', 'dark'] as const).map(scheme => html`
        <button data-scheme="${scheme}"
            ?selected="${this.theme_.scheme === scheme}"
            @click="${this.onSchemeClick_}">
          <span class="icon" style="mask-image: url(sidebar/ui_icons/${
              {auto: 'sparkles', light: 'sunny', dark: 'moon'}[scheme]}.svg)">
          </span>
        </button>`)}
    </div>
    <div id="actions" class="buttons">
      <button id="add" ?disabled="${!this.canAdd_()}" @click="${this.onAddClick_}">
        <span class="icon" style="mask-image: url(sidebar/ui_icons/add.svg)"></span>
      </button>
      <button id="remove" ?disabled="${this.isDefault_()}"
          @click="${this.onRemoveClick_}">
        <span class="icon" style="mask-image: url(sidebar/ui_icons/remove.svg)"></span>
      </button>
      <button id="toggle" ?disabled="${!this.canToggle_()}"
          @click="${this.onToggleClick_}">
        <span class="icon" style="mask-image: url(sidebar/ui_icons/color-filter.svg)"></span>
      </button>
    </div>
    <span id="click-to-add" ?hidden="${!this.isDefault_()}">Щёлкните, чтобы добавить цвет</span>
    ${this.dots_.map(dot => html`
      <div class="dot" ?primary="${dot.id === 0}"
          ?dragging="${dot.id === 0 && this.dragging_}"
          style="left: ${this.shownAt_(dot).x}px; top: ${this.shownAt_(dot).y}px; --dot-color: rgb(${dot.color.join(', ')})"
          @pointerdown="${this.onDotPointerdown_}"
          @pointermove="${this.onDotPointermove_}"
          @pointerup="${this.onDotPointerup_}"
          @contextmenu="${this.onDotContextmenu_}"></div>`)}
  </div>
  <div id="pages-row">
    <button class="page-button" ?disabled="${this.page_ === 0}"
        @click="${this.onPageLeftClick_}">
      <span class="icon" style="mask-image: url(sidebar/ui_icons/chevron-back.svg)"></span>
    </button>
    <div id="pages" @wheel="${this.onPagesWheel_}">
      ${this.pages_.map((page, p) => html`
        <div class="page">
          ${page.map((preset, i) => html`
            <div class="swatch" data-preset="${p},${i}"
                style="${swatchStyle(preset)}"
                @click="${this.onPresetClick_}"></div>`)}
        </div>`)}
    </div>
    <button class="page-button" ?disabled="${this.page_ === this.pages_.length - 1}"
        @click="${this.onPageRightClick_}">
      <span class="icon" style="mask-image: url(sidebar/ui_icons/chevron-forward.svg)"></span>
    </button>
  </div>
  <div id="controls">
    <div id="colors-wrapper">
      <div id="opacity-wrapper">
        <div id="wave">
          <svg viewBox="0 -7.605 455 70" xmlns="http://www.w3.org/2000/svg"
              preserveAspectRatio="xMinYMid meet">
            <path d="${this.wavePath_()}" fill="none" stroke-linecap="round"
                stroke-linejoin="round"
                class="${this.waveIsLine_() ? 'line' : 'wave'}"/>
            <defs>
              <linearGradient id="wave-gradient" x1="0%" y1="0%" x2="100%" y2="0%">
                <stop offset="0%" class="stop-full"/>
                <stop offset="${this.progress_() * 100}%" class="stop-full"/>
                <stop offset="${this.progress_() * 100}%" class="stop-faint"/>
              </linearGradient>
            </defs>
          </svg>
        </div>
        <input id="opacity" type="range" min="0.3" max="0.8" step="0.001"
            .value="${String(this.theme_.opacity)}"
            style="${this.thumbStyle_()}"
            @input="${this.onOpacityInput_}">
      </div>
      <div id="texture" style="--grain: ${this.theme_.texture}">
        ${[...Array(16).keys()].map(i => html`
          <div class="texture-dot" ?active="${textureDotLit(i, this.theme_.texture)}"
              style="${this.textureDotStyle_(i)}">
          </div>`)}
        <div id="texture-handle" style="${this.textureHandleStyle_()}"
            @pointerdown="${this.onTexturePointerdown_}"
            @pointermove="${this.onTexturePointermove_}"></div>
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
