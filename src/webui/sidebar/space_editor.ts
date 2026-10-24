// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import './icon_picker.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {ICloudSpace} from '../sidebar.mojom-webui.js';

import {iconMask} from './space_icon.js';
import {getCss} from './space_editor.css.js';
import {getHtml} from './space_editor.html.js';

export interface SpaceEdit {
  name: string;
  icon: string;
}

// Zen's space creation form (zen:spaces/ZenSpaceCreation.mjs,
// create-workspace-form.css), shown in the sidebar over the strip; also the
// form for changing a space's name and icon. Fires `space-edit-done` with a
// SpaceEdit, `space-edit-cancel` (Esc, as in Zen) or `space-edit-theme`.
export class SpaceEditorElement extends CrLitElement {
  static get is() {
    return 'cromarc-space-editor';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      creating: {type: Boolean},
      name: {type: String},
      icon: {type: String},
      pickerOpen: {type: Boolean},
      remoteSpaces: {type: Array},
    };
  }

  accessor creating: boolean = true;
  accessor name: string = '';
  accessor icon: string = '';
  accessor pickerOpen: boolean = false;
  // Spaces in iCloud Drive this Mac does not have yet (sync between Macs).
  accessor remoteSpaces: ICloudSpace[] = [];

  override firstUpdated(changed: PropertyValues<this>) {
    super.firstUpdated(changed);
    if (!this.pickerOpen) {
      this.shadowRoot.querySelector<HTMLInputElement>('#name')!.focus();
    }
  }

  protected iconMask_(): string {
    return iconMask(this.icon);
  }

  protected onIconClick_() {
    this.pickerOpen = !this.pickerOpen;
  }

  protected onIconPicked_(e: CustomEvent<string>) {
    this.icon = e.detail;
    this.pickerOpen = false;
  }

  protected onNameInput_(e: Event) {
    this.name = (e.target as HTMLInputElement).value;
  }

  protected onFormKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      e.preventDefault();
      this.fire('space-edit-cancel');
    } else if (e.key === 'Enter' && !this.pickerOpen) {
      e.preventDefault();
      this.onDoneClick_();
    }
  }

  protected onDoneClick_() {
    const name = this.name.trim();
    if (!name) {
      this.shadowRoot.querySelector<HTMLInputElement>('#name')!.focus();
      return;
    }
    this.fire('space-edit-done', {name, icon: this.icon} satisfies SpaceEdit);
  }

  protected remoteIcon_(space: ICloudSpace): string {
    return iconMask(space.icon) ? '' : space.icon;
  }

  protected onConnectClick_(e: Event) {
    const uuid = (e.currentTarget as HTMLElement).dataset['uuid'];
    const space = this.remoteSpaces.find(s => s.uuid === uuid);
    if (space) {
      this.fire('space-connect', space);
    }
  }

  protected onCancelClick_() {
    this.fire('space-edit-cancel');
  }

  // Zen's "Edit theme" in the form (zen-workspaces-change-theme).
  protected onThemeClick_() {
    this.fire('space-edit-theme');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-space-editor': SpaceEditorElement;
  }
}

customElements.define(SpaceEditorElement.is, SpaceEditorElement);
