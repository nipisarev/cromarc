// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryMediaElement} from './library_media.js';

export function getHtml(this: LibraryMediaElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.enabled_ ? html`
  <div class="search-top" ?open="${this.filtersOpen_}">
    <div class="search-header" ?inert="${this.filtersOpen_}">
      <cromarc-library-search-box class="search-box"
          placeholder="${this.strings.mediaSearch}"
          @search-changed="${this.onSearchChanged_}">
      </cromarc-library-search-box>
      ${this.hasFilters_() ? html`
        <button class="filter-button" @click="${this.onFilterButtonClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/options.svg)"></span>
          <span>${this.strings.filter}</span>
        </button>` : nothing}
    </div>
    <div class="filter-header" ?inert="${!this.filtersOpen_}">
      <h2>${this.strings.mediaFilterTitle}</h2>
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
  </div>` : nothing}
<div class="search-results">
  ${!this.enabled_ ? html`
    <div class="media-skeleton" behind></div>
    <div class="media-opt-in">
      <div class="media-opt-in-icon">
        <div class="tab-icon-image"
            style="background-image: ${this.optInSprite_()}"></div>
      </div>
      <h3>${this.strings.mediaOptInTitle}</h3>
      <p>${this.strings.mediaOptInBody}</p>
      <button class="big-accent-button media-opt-in-button"
          @click="${this.onEnableClick_}">
        ${this.strings.mediaOptInButton}
      </button>
      <div class="media-opt-in-note">
        <span class="svg" style="mask-image: url(sidebar/ui_icons/lock.svg)">
        </span>
        <span>${this.strings.mediaOptInNote}</span>
      </div>
    </div>` : html`
    ${this.loading_ ? html`
      <div class="empty">${this.strings.mediaLoading}</div>` : html`
      ${!this.shown_().length ? html`
        <div class="empty">${this.strings.mediaEmpty}</div>` : html`
        <div class="media-grid">
          ${this.shown_().map(item => html`
            <div class="media-item" data-id="${item.id}" title="${item.name}"
                @click="${this.onItemClick_}"
                @contextmenu="${this.onItemContextmenu_}">
              <div class="media-frame" data-kind="${item.kind}">
                <img class="media-thumb" alt="" loading="lazy"
                    draggable="false" ?placeholder="${item.cloudPlaceholder}"
                    src="${this.thumbSrc_(item)}">
                ${item.kind === 'video' ? html`
                  <span class="svg media-play"
                      style="mask-image: url(sidebar/ui_icons/play.svg)"></span>` :
                  nothing}
              </div>
            </div>`)}
        </div>`}`}`}
</div>
${this.previewItem_() ? html`
  <div class="media-preview">
    <div class="media-preview-backdrop" @click="${this.onPreviewBackdropClick_}">
    </div>
    <div class="media-preview-panel">
      <div class="media-preview-header">
        <button class="row-action" title="${this.strings.mediaClose}"
            @click="${this.onPreviewCloseClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/close.svg)"></span>
        </button>
        <button class="row-action" title="${this.strings.mediaPrevious}"
            ?disabled="${this.preview_ <= 0}"
            @click="${this.onPreviewPrevClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/chevron-back.svg)"></span>
        </button>
        <button class="row-action" title="${this.strings.mediaNext}"
            ?disabled="${this.preview_ >= this.shown_().length - 1}"
            @click="${this.onPreviewNextClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/chevron-forward.svg)">
          </span>
        </button>
        <span class="media-preview-name">${this.previewItem_()?.name}</span>
        <span class="media-preview-date">${this.previewDate_()}</span>
        <button class="row-action" title="${this.strings.mediaMore}"
            @click="${this.onPreviewMoreClick_}">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/ellipsis.svg)"></span>
        </button>
        <button class="media-preview-open" @click="${this.onPreviewOpenClick_}">
          ${this.openLabel_()}
        </button>
      </div>
      <div class="media-preview-stage" @contextmenu="${this.onStageContextmenu_}">
        ${this.previewIsVideo_() ? html`
          <video class="media-preview-media" src="${this.previewSrc_()}"
              controls autoplay></video>` : html`
          <img class="media-preview-media" alt="" draggable="false"
              src="${this.previewSrc_()}">`}
      </div>
    </div>
  </div>` : nothing}
<!--_html_template_end_-->`;
  // clang-format on
}
