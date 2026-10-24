// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryElement} from './library.js';

export function getHtml(this: LibraryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="library" @keydown="${this.onKeydown_}">
  <div id="header">
    <span class="title">Библиотека</span>
    <button id="close" title="Закрыть (Esc)" @click="${this.onCloseClick_}">✕</button>
  </div>
  <div id="sections">
    <button class="section" ?selected="${this.section_ === 'archive'}"
        @click="${this.onArchiveClick_}">🗃 Архив</button>
    <button class="section" ?selected="${this.section_ === 'links'}"
        @click="${this.onLinksClick_}">Ссылки</button>
    <button class="section" @click="${this.onHistoryClick_}">История</button>
    <button class="section" @click="${this.onDownloadsClick_}">Загрузки</button>
  </div>
  ${this.section_ === 'links' ? html`
  <div id="links">
    <label class="hint">Ссылки из других приложений открываются в</label>
    <select id="default-route" @change="${this.onDefaultRouteChange_}">
      <option value="" ?selected="${this.defaultRoute_ === ''}">
        последнем открытом space
      </option>
      ${this.spaces.map(space => html`
        <option value="${space.id}"
            ?selected="${this.defaultRoute_ === space.id}">${space.name}</option>`)}
    </select>
    <div class="hint">…кроме этих правил (первое подходящее):</div>
    ${this.routes_.map(route => html`
      <div class="route">
        <span class="reference">${route.reference}</span>
        <span class="sub">${this.matchLabels_[route.match]} →
          ${this.spaceName_(route.space)}</span>
        <button class="remove" data-id="${route.id}" title="Удалить правило"
            @click="${this.onRemoveRouteClick_}">✕</button>
      </div>`)}
    <div id="add-route">
      <input id="route-reference" placeholder="github.com/qase" spellcheck="false">
      <div class="row-controls">
        <select id="route-match">
          <option value="contains">содержит</option>
          <option value="equal-to">равно</option>
          <option value="regex">regex</option>
        </select>
        <select id="route-space">
          ${this.spaces.map(space => html`
            <option value="${space.id}">${space.name}</option>`)}
        </select>
        <button id="add-route-button" @click="${this.onAddRouteClick_}">
          Добавить
        </button>
      </div>
    </div>
  </div>` : html`
  <input id="search" type="search" placeholder="Поиск в архиве"
      spellcheck="false" .value="${this.query_}"
      @input="${this.onSearchInput_}">
  <div id="list">
    <p class="empty" ?hidden="${!this.loaded_ || this.groups_.length > 0}">
      ${this.query_ ? 'Ничего не нашлось' : 'Архив пуст'}
    </p>
    ${this.groups_.map(group => html`
      <div class="day">${group.label}</div>
      ${group.tabs.map(tab => html`
        <div class="row" data-id="${tab.id}" title="${tab.url}"
            @click="${this.onRestoreClick_}">
          <span class="favicon"
              style="background-image: ${this.faviconFor_(tab)}"></span>
          <span class="text">
            <span class="name">${tab.title || tab.url}</span>
            <span class="sub">${this.subtitleFor_(tab)}</span>
          </span>
          <span class="restore" title="Вернуть в Today">↺</span>
        </div>`)}`)}
  </div>`}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
