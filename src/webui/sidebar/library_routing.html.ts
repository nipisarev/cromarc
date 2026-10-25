// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryRoutingElement} from './library_routing.js';

export function getHtml(this: LibraryRoutingElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="search-top" ?open="${this.formOpen_}">
  <div class="search-header" ?inert="${this.formOpen_}">
    <cromarc-library-search-box class="search-box"
        placeholder="${this.strings.routingSearch}"
        @search-changed="${this.onSearchChanged_}">
    </cromarc-library-search-box>
    <button class="filter-button" @click="${this.onNewClick_}">
      <span class="svg" style="mask-image: url(sidebar/ui_icons/add.svg)">
      </span>
      <span>${this.strings.routingNew}</span>
    </button>
  </div>
  <div class="filter-header" ?inert="${!this.formOpen_}">
    <h2>${this.draft_?.id ? this.strings.routingEditTitle :
                            this.strings.routingNewTitle}</h2>
  </div>
  <div class="filter-panel">
    <div class="filter-panel-inner" ?inert="${!this.formOpen_}">
      ${this.draft_ ? html`
        <div class="routing-form">
          <div class="filter-group">
            <h3>${this.strings.routingWhenTheLink}</h3>
            <div class="routing-match-types">
              ${this.matchTypes_.map(type => html`
                <button class="filter-chip" data-match="${type.id}"
                    ?active="${this.draft_?.match === type.id}"
                    @click="${this.onMatchClick_}">${type.label}</button>`)}
            </div>
            <input class="routing-reference" type="text" spellcheck="false"
                .value="${this.draft_.reference}"
                placeholder="${this.draft_.match === 'regex' ?
                    'github\\.com/acme' : 'github.com/acme'}"
                ?invalid="${this.referenceInvalid_()}"
                @input="${this.onReferenceInput_}"
                @keydown="${this.onReferenceKeydown_}">
          </div>
          <div class="filter-group">
            <h3>${this.strings.routingOpenItIn}</h3>
            <button class="routing-select" @click="${this.onDraftSpaceClick_}">
              ${this.spaceIconMask_(this.draft_.space) ? html`
                <span class="svg" style="mask-image: ${
                    this.spaceIconMask_(this.draft_.space)}"></span>` :
                html`<span class="routing-space-icon">${
                    this.spaceGlyph_(this.draft_.space)}</span>`}
              <span class="routing-select-name">${
                  this.destinationLabel_(this.draft_.space)}</span>
              <span class="svg routing-chevron"
                  style="mask-image: url(sidebar/ui_icons/chevron-down.svg)">
              </span>
            </button>
          </div>
          <div class="routing-form-buttons">
            <button @click="${this.onCancelClick_}">
              ${this.strings.routingCancel}
            </button>
            <button class="routing-save" ?disabled="${!this.draftIsValid_()}"
                @click="${this.onSaveClick_}">
              ${this.draft_.id ? this.strings.routingSave :
                                 this.strings.routingAdd}
            </button>
          </div>
        </div>` : nothing}
    </div>
  </div>
</div>
<div class="search-results">
  ${!this.shownRoutes_().length ? html`
    <div class="empty">${this.routes_.length ? this.strings.routingNoResults :
                                               this.strings.routingEmpty}
    </div>` : nothing}
  ${this.shownRoutes_().map(route => html`
    <div class="row routing-row" data-id="${route.id}"
        ?active="${this.draft_?.id === route.id}"
        @click="${this.onRouteClick_}">
      <div class="routing-lines">
        <div class="routing-line">
          <span class="svg" style="mask-image: url(sidebar/ui_icons/link.svg)">
          </span>
          <span class="routing-match">${this.matchLabel_(route.match)}</span>
          <span class="routing-value">${route.reference}</span>
        </div>
        <div class="routing-line routing-destination">
          <span class="svg"
              style="mask-image: url(sidebar/ui_icons/return-down-forward.svg)">
          </span>
          ${this.spaceIconMask_(route.space) ? html`
            <span class="svg" style="mask-image: ${
                this.spaceIconMask_(route.space)}"></span>` :
            html`<span class="routing-space-icon">${
                this.spaceGlyph_(route.space)}</span>`}
          <span class="routing-value">${this.destinationLabel_(route.space)}
          </span>
        </div>
      </div>
      <div class="row-actions">
        <button class="row-action" title="${this.strings.routingRemove}"
            data-id="${route.id}" @click="${this.onRemoveClick_}">
          <span class="svg" style="mask-image: url(sidebar/ui_icons/trash.svg)">
          </span>
        </button>
      </div>
    </div>`)}
  <div class="search-sentinel"></div>
</div>
<div class="routing-footer">
  <h3>${this.strings.routingExternalLinks}</h3>
  <button class="routing-select" @click="${this.onExternalSpaceClick_}">
    ${this.spaceIconMask_(this.defaultExternal_) ? html`
      <span class="svg" style="mask-image: ${
          this.spaceIconMask_(this.defaultExternal_)}"></span>` :
      html`<span class="routing-space-icon">${
          this.spaceGlyph_(this.defaultExternal_)}</span>`}
    <span class="routing-select-name">${
        this.destinationLabel_(this.defaultExternal_)}</span>
    <span class="svg routing-chevron"
        style="mask-image: url(sidebar/ui_icons/chevron-down.svg)"></span>
  </button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
