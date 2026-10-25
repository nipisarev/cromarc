// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './library_search_box.html.js';

const SEARCH_DEBOUNCE_MS = 250;

// Zen's .zen-library-search-box: fires `search-changed` with the trimmed
// query a quarter second after typing stops. Draws into the Library's shadow
// root (library.css).
export class LibrarySearchBoxElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-search-box';
  }

  override createRenderRoot() {
    return this;
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      placeholder: {type: String},
    };
  }

  accessor placeholder: string = '';
  private query_ = '';
  private timer_ = 0;

  override disconnectedCallback() {
    super.disconnectedCallback();
    clearTimeout(this.timer_);
  }

  override focus() {
    this.querySelector('input')?.focus();
  }

  protected onInput_(e: Event) {
    const value = (e.target as HTMLInputElement).value;
    clearTimeout(this.timer_);
    this.timer_ = setTimeout(() => {
      const query = value.trim();
      if (query !== this.query_) {
        this.query_ = query;
        this.fire('search-changed', query);
      }
    }, SEARCH_DEBOUNCE_MS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-search-box': LibrarySearchBoxElement;
  }
}

customElements.define(LibrarySearchBoxElement.is, LibrarySearchBoxElement);
