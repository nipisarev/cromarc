// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ArchivedTab} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './library.css.js';
import {getHtml} from './library.html.js';
import {groupByDay, reasonLabel, timeLabel} from './library_model.js';
import type {DayGroup} from './library_model.js';

const LIMIT = 200;

// The Library (design, UI section 10), after Zen's ZenLibrary: Cromarc's
// archive first, by day, searchable; History and Downloads open Chrome's
// pages. Fires `library-close`.
export class LibraryElement extends CrLitElement {
  static get is() {
    return 'cromarc-library';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      query_: {type: String},
      groups_: {type: Array},
      loaded_: {type: Boolean},
    };
  }

  protected accessor query_: string = '';
  protected accessor groups_: Array<DayGroup<ArchivedTab>> = [];
  protected accessor loaded_: boolean = false;
  private readonly proxy_ = BrowserProxy.getInstance();
  private request_ = 0;

  override connectedCallback() {
    super.connectedCallback();
    this.search_();
  }

  protected faviconFor_(tab: ArchivedTab): string {
    return getFaviconForPageURL(tab.url, false);
  }

  protected subtitleFor_(tab: ArchivedTab): string {
    return `${reasonLabel(tab.reason)} · ${timeLabel(tab.closedAt)}`;
  }

  protected onSearchInput_(e: Event) {
    this.query_ = (e.target as HTMLInputElement).value;
    this.search_();
  }

  protected onRestoreClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['id'];
    const tab = this.groups_.flatMap(g => g.tabs)
                    .find(t => String(t.id) === id);
    if (!tab) {
      return;
    }
    this.proxy_.handler.restoreFromArchive(tab.id, tab.url);
    this.groups_ = this.groups_
                       .map(g => ({...g, tabs: g.tabs.filter(t => t !== tab)}))
                       .filter(g => g.tabs.length);
  }

  protected onHistoryClick_() {
    this.openPage_('chrome://history/');
  }

  protected onDownloadsClick_() {
    this.openPage_('chrome://downloads/');
  }

  protected onCloseClick_() {
    this.fire('library-close');
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.fire('library-close');
    }
  }

  private openPage_(url: string) {
    this.proxy_.tabStrip.createTabAt(null, url).catch(() => {});
    this.fire('library-close');
  }

  private async search_() {
    const request = ++this.request_;
    const {tabs} = await this.proxy_.handler.searchArchive(this.query_, LIMIT);
    if (request !== this.request_) {
      return;
    }
    this.groups_ = groupByDay(tabs, Date.now());
    this.loaded_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library': LibraryElement;
  }
}

customElements.define(LibraryElement.is, LibraryElement);
