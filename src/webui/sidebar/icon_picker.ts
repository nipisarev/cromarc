// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {EMOJIS} from './emoji_data.js';
import {getCss} from './icon_picker.css.js';
import {getHtml} from './icon_picker.html.js';
import {SPACE_ICONS} from './space_icons.js';
import {iconValue} from './space_icon.js';

// Zen's emoji picker (zen:common/emojis/ZenEmojiPicker.mjs): emojis searched
// by tag, a page of Zen's icons, and "none". Fires `icon-picked` with the
// value: an emoji, "icon:<name>" or "".
export class IconPickerElement extends CrLitElement {
  static get is() {
    return 'cromarc-icon-picker';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      page_: {type: String},
      query_: {type: String},
      icons_: {type: Array},
    };
  }

  protected accessor page_: 'emojis'|'icons' = 'emojis';
  protected accessor query_: string = '';
  protected accessor icons_: readonly string[] = SPACE_ICONS;

  protected emojis_(): string[] {
    const query = this.query_.trim().toLowerCase();
    return EMOJIS.filter(([, tags]) => !query || tags.includes(query))
        .map(([emoji]) => emoji);
  }

  protected iconUrl_(name: string): string {
    return `url(sidebar/icons/${name}.svg)`;
  }

  protected onSearchInput_(e: Event) {
    this.query_ = (e.target as HTMLInputElement).value;
    this.page_ = 'emojis';
  }

  protected onEmojisPageClick_() {
    this.page_ = 'emojis';
  }

  protected onIconsPageClick_() {
    this.page_ = 'icons';
  }

  protected onEmojiClick_(e: Event) {
    this.pick_((e.currentTarget as HTMLElement).dataset['value']!);
  }

  protected onIconClick_(e: Event) {
    this.pick_(iconValue((e.currentTarget as HTMLElement).dataset['value']!));
  }

  protected onNoneClick_() {
    this.pick_('');
  }

  private pick_(value: string) {
    this.fire('icon-picked', value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-icon-picker': IconPickerElement;
  }
}

customElements.define(IconPickerElement.is, IconPickerElement);
