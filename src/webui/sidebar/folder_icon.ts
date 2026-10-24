// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './folder_icon.css.js';
import {getHtml} from './folder_icon.html.js';

// Zen's folder icon (zen:folders/ZenFolder.mjs rawIcon), coloured by the
// --zen-folder-* variables of the sidebar; its sheets part when `open`.
export class FolderIconElement extends CrLitElement {
  static get is() {
    return 'cromarc-folder-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      open: {type: Boolean, reflect: true},
    };
  }

  accessor open: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-folder-icon': FolderIconElement;
  }
}

customElements.define(FolderIconElement.is, FolderIconElement);
