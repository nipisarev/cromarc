// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LibraryElement} from './library.js';

export function getHtml(this: LibraryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="sprite-probe"></div>
<div id="panel" ?customizing="${this.customizing_}">
  <div id="side">
    <div id="header"></div>
    <div id="tabs">
      ${this.railTabs_().map(tab => html`
        <div class="tab" data-section="${tab.id}"
            ?active="${tab.id === this.activeTab_}"
            ?hidden-tab="${this.isHidden_(tab.id)}"
            ?animate="${tab.id === this.animatingTab_}"
            ?downloading="${tab.id === 'downloads' && this.downloading_}"
            @click="${this.onTabClick_}"
            @contextmenu="${this.onTabContextmenu_}">
          <div class="tab-icon">
            <div class="tab-icon-image"
                style="background-image: ${this.spriteFor_(tab.id)}"></div>
          </div>
          <span class="tab-label">${this.tabLabel_(tab)}</span>
          ${this.customizing_ ? html`
            <button class="tab-toggle" data-section="${tab.id}"
                title="${this.isHidden_(tab.id) ? this.strings.restoreTab :
                                                  this.strings.removeTab}"
                @click="${this.onTabToggleClick_}">
              <span class="svg" style="mask-image: url(sidebar/ui_icons/${
                  this.isHidden_(tab.id) ? 'add' : 'close'}.svg)"></span>
            </button>` : nothing}
        </div>`)}
    </div>
    <div id="footer">
      <button class="footer-button" title="${this.strings.close}"
          @click="${this.onCloseClick_}">
        <span class="svg"
            style="mask-image: url(sidebar/ui_icons/arrow-back.svg)"></span>
      </button>
      <button class="footer-button" title="${this.strings.customize}"
          ?checked="${this.customizing_}" @click="${this.onCustomizeClick_}">
        <span class="svg"
            style="mask-image: url(sidebar/ui_icons/brush.svg)"></span>
      </button>
    </div>
  </div>
  <div id="content" @closed-tabs-changed="${this.onClosedTabsChanged_}">
    ${this.isMounted_('space-routing') ? html`
      <cromarc-library-routing class="section" data-section="space-routing"
          ?hidden="${this.activeTab_ !== 'space-routing'}"
          .library="${this}" .spaces="${this.spaces}">
      </cromarc-library-routing>` : nothing}
    ${this.isMounted_('downloads') ? html`
      <cromarc-library-downloads class="section" data-section="downloads"
          ?hidden="${this.activeTab_ !== 'downloads'}" .library="${this}">
      </cromarc-library-downloads>` : nothing}
    ${this.isMounted_('history') ? html`
      <cromarc-library-history class="section" data-section="history"
          ?hidden="${this.activeTab_ !== 'history'}" .library="${this}">
      </cromarc-library-history>` : nothing}
  </div>
</div>
${this.toast_ ? html`
  <div id="toast" role="status">${this.toast_}</div>` : nothing}
${this.confirm_ ? html`
  <div id="confirm-backdrop">
    <div id="confirm" role="alertdialog">
      <h3>${this.confirm_.title}</h3>
      <p>${this.confirm_.message}</p>
      <div class="confirm-buttons">
        <button class="pill" @click="${this.onConfirmCancelClick_}">
          ${this.strings.cancel}
        </button>
        <button class="pill strong" @click="${this.onConfirmAcceptClick_}">
          ${this.confirm_.accept}
        </button>
      </div>
    </div>
  </div>` : nothing}
<!--_html_template_end_-->`;
  // clang-format on
}
