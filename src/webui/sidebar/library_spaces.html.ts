// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibrarySpacesElement} from './library_spaces.js';

export function getHtml(this: LibrarySpacesElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="spaces">
  ${this.spaces.map(space => html`
    <div class="space-card" data-space="${space.id}"
        ?active="${space.current}" ?dragging="${this.dragging_ === space.id}"
        style="${this.cardStyle_(space)}">
      <div class="space-card-header">
        <button class="space-card-icon" data-space="${space.id}"
            ?no-icon="${!space.icon}" title="${this.strings.spaceIcon}"
            @click="${this.onIconClick_}">
          ${this.iconMask_(space) ? html`
            <span class="svg" style="mask-image: ${this.iconMask_(space)}">
            </span>` : html`${this.glyph_(space)}`}
        </button>
        ${this.renaming_ === space.id ? html`
          <input class="space-card-name-input" data-space="${space.id}"
              .value="${space.name}" spellcheck="false"
              @keydown="${this.onNameKeydown_}" @blur="${this.onNameBlur_}">` :
          html`
          <button class="space-card-name" data-space="${space.id}"
              title="${this.strings.spaceRename}"
              @click="${this.onNameClick_}">${space.name}</button>`}
        <button class="row-action" data-space="${space.id}"
            title="${this.strings.spaceTheme}" @click="${this.onThemeClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/brush.svg)"></span>
        </button>
        ${this.picking_ === space.id ? html`
          <cromarc-icon-picker class="space-card-picker"
              @icon-picked="${this.onIconPicked_}">
          </cromarc-icon-picker>` : nothing}
      </div>
      <div class="space-card-body">
        ${this.visibleRows_(space).map(row => html`
          <div class="space-row" data-space="${space.id}" data-row="${row.id}"
              ?folder="${row.kind === 'folder'}"
              style="${this.rowIndent_(row)}" title="${row.title}"
              @click="${this.onRowClick_}">
            ${row.kind === 'folder' ? html`
              <cromarc-folder-icon class="space-row-folder"
                  ?open="${!this.isCollapsed_(row)}"></cromarc-folder-icon>` :
              html`
              <span class="space-row-icon"
                  style="background-image: ${this.rowFavicon_(space, row)}"></span>`}
            <span class="space-row-title">${row.title}</span>
          </div>`)}
        ${this.visibleRows_(space).length && this.cardOf_(space).today.length ?
            html`<div class="space-card-separator"></div>` : nothing}
        ${this.cardOf_(space).today.map(tab => html`
          <div class="space-row" data-space="${space.id}" data-url="${tab.url}"
              title="${tab.title}" @click="${this.onRowClick_}">
            <span class="space-row-icon"
                style="background-image: ${this.todayFavicon_(tab)}"></span>
            <span class="space-row-title">${tab.title || tab.url}</span>
          </div>`)}
      </div>
      <div class="space-card-footer">
        <button class="row-action space-card-handle" data-space="${space.id}"
            title="${this.strings.spaceMove}"
            @pointerdown="${this.onHandlePointerdown_}"
            @pointermove="${this.onHandlePointermove_}"
            @pointerup="${this.onHandlePointerup_}"
            @pointercancel="${this.onHandlePointercancel_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/move.svg)"></span>
        </button>
        <button class="row-action" data-space="${space.id}"
            title="${this.strings.spaceActions}"
            @click="${this.onActionsClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/ellipsis.svg)"></span>
        </button>
      </div>
    </div>`)}
  <div class="add-space">
    <button class="add-space-badge" title="${this.strings.spaceCreate}"
        @click="${this.onAddClick_}">
      <svg class="add-space-ring" viewBox="0 0 30 30">
        <circle class="add-space-arc" cx="15" cy="15" pathLength="100">
        </circle>
      </svg>
      <span class="svg add-space-icon"
          style="mask-image: url(sidebar/ui_icons/add.svg)"></span>
    </button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
