// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryHistoryElement} from './library_history.js';

export function getHtml(this: LibraryHistoryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="search-top" ?open="${this.filtersOpen_}">
  <div class="search-header" ?inert="${this.filtersOpen_}">
    <cromarc-library-search-box class="search-box"
        placeholder="${this.strings.historySearch}"
        @search-changed="${this.onSearchChanged_}">
    </cromarc-library-search-box>
    <button class="filter-button" @click="${this.onFilterButtonClick_}">
      <span class="svg"
          style="mask-image: url(sidebar/ui_icons/options.svg)"></span>
      <span>${this.strings.filter}</span>
    </button>
  </div>
  <div class="filter-header" ?inert="${!this.filtersOpen_}">
    <h2>${this.strings.historyFilterTitle}</h2>
    <button class="filter-done" @click="${this.onFilterDoneClick_}">
      ${this.strings.done}
    </button>
  </div>
  <div class="filter-panel">
    <div class="filter-panel-inner" ?inert="${!this.filtersOpen_}">
      <cromarc-library-filters .groups="${this.filterGroups_()}"
          .active="${this.active_}"
          @filter-toggled="${this.onFilterToggled_}">
      </cromarc-library-filters>
    </div>
  </div>
</div>
<div class="search-results">
  ${this.isEmpty_() ? html`
    <div class="empty">${this.showingClosed_() ? this.strings.closedEmpty :
                                                 this.strings.historyEmpty}
    </div>` : nothing}
  ${this.groups_.map(group => html`
    <div class="group">
      ${this.groupTitle_(group) ? html`
        <h3>${this.groupTitle_(group)}</h3>` : nothing}
      ${group.visits.map(visit => html`
        <div class="row" draggable="true" data-key="${this.visitKey_(visit)}"
            ?menu-open="${this.menuRow_ === this.visitKey_(visit)}"
            @click="${this.onRowClick_}" @auxclick="${this.onRowAuxclick_}"
            @contextmenu="${this.onRowContextmenu_}"
            @dragstart="${this.onRowDragstart_}">
          <span class="row-icon"
              style="background-image: ${this.faviconFor_(visit)}"></span>
          <div class="row-text">
            <span class="row-title">${visit.title || visit.url}</span>
            <span class="row-subtitle"><span class="visit-url">${
                this.urlLabel_(visit)}</span><span class="visit-date">${
                this.timeLabel_(visit)}</span></span>
          </div>
          <div class="row-actions">
            <button class="row-action" title="${this.strings.forgetButton}"
                data-key="${this.visitKey_(visit)}"
                @click="${this.onForgetClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/trash.svg)"></span>
            </button>
            <button class="row-action" title="${this.strings.reopenButton}"
                data-key="${this.visitKey_(visit)}"
                @click="${this.onReopenClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/arrow-undo.svg)">
              </span>
            </button>
          </div>
        </div>`)}
    </div>`)}
  <div class="search-sentinel"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
