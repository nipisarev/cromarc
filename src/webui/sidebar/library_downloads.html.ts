// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryDownloadsElement} from './library_downloads.js';

export function getHtml(this: LibraryDownloadsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="search-top" ?open="${this.filtersOpen_}">
  <div class="search-header" ?inert="${this.filtersOpen_}">
    <cromarc-library-search-box class="search-box"
        placeholder="${this.strings.downloadsSearch}"
        @search-changed="${this.onSearchChanged_}">
    </cromarc-library-search-box>
    <button class="filter-button" @click="${this.onFilterButtonClick_}">
      <span class="svg"
          style="mask-image: url(sidebar/ui_icons/options.svg)"></span>
      <span>${this.strings.filter}</span>
    </button>
  </div>
  <div class="filter-header" ?inert="${!this.filtersOpen_}">
    <h2>${this.strings.downloadsFilterTitle}</h2>
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
    <div class="empty">${this.strings.downloadsEmpty}</div>` : nothing}
  <div class="group">
    ${this.visible_().map(download => html`
      <div class="row download-row" draggable="true"
          data-id="${download.id}"
          ?pending="${this.isPending_(download)}"
          ?missing="${download.missing}"
          ?indeterminate="${this.isPending_(download) &&
                            !this.hasProgress_(download)}"
          ?paused="${this.isPending_(download) && download.paused}"
          ?menu-open="${this.menuRow_ === download.id}"
          @click="${this.onRowClick_}"
          @contextmenu="${this.onRowContextmenu_}"
          @dragstart="${this.onRowDragstart_}">
        <div class="row-icon-box">
          ${this.isPending_(download) ? html`
            <span class="svg pending-icon"
                style="mask-image: url(sidebar/ui_icons/arrow-back.svg)">
            </span>
            <svg class="row-progress" viewBox="0 0 32 32">
              <circle class="row-progress-track" cx="16" cy="16" r="14">
              </circle>
              <circle class="row-progress-arc" cx="16" cy="16" r="14"
                  pathLength="100"
                  style="stroke-dasharray: ${this.ringDash_(download)}">
              </circle>
            </svg>` : html`
            <span class="row-icon file-icon"
                ?preview="${!!download.previewUrl}"
                style="background-image: ${this.iconFor_(download)}"></span>`}
        </div>
        <div class="row-text">
          <span class="row-title">${download.fileName}</span>
          <span class="row-subtitle"><span class="download-status">${
              this.statusLabel_(download)}</span><span class="download-url">${
              this.urlLabel_(download)}</span></span>
        </div>
        <div class="row-actions">
          ${this.isPending_(download) ? html`
            <button class="row-action" title="${this.strings.cancelDownload}"
                data-id="${download.id}" @click="${this.onCancelClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/close.svg)"></span>
            </button>` : nothing}
          ${this.canRetry_(download) ? html`
            <button class="row-action" title="${this.strings.retryDownload}"
                data-id="${download.id}" @click="${this.onRetryClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/refresh.svg)"></span>
            </button>` : nothing}
          ${this.hasFile_(download) ? html`
            <button class="row-action" title="${this.strings.showInFinder}"
                data-id="${download.id}" @click="${this.onShowClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/search.svg)"></span>
            </button>` : nothing}
          <button class="row-action" title="${this.strings.moreOptions}"
              data-id="${download.id}" @click="${this.onMoreClick_}">
            <span class="svg"
                style="mask-image: url(sidebar/ui_icons/ellipsis.svg)"></span>
          </button>
        </div>
      </div>`)}
  </div>
  <div class="search-sentinel"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
