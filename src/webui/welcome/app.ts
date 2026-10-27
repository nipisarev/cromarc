// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import '../sidebar/icon_picker.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageHandlerFactory, PageHandlerRemote} from '../welcome.mojom-webui.js';
import {s} from '../i18n/i18n.js';
import {iconMask} from '../sidebar/space_icon.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {COLORS, cssColor, nextStep, stepsFor} from './steps.js';
import type {Step} from './steps.js';

// Cromarc's first run (design, UI section 12): the first space, more spaces,
// Arc's sidebar, the default browser.
export class WelcomeAppElement extends CrLitElement {
  static get is() {
    return 'cromarc-welcome-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      step_: {type: String},
      steps_: {type: Array},
      name_: {type: String},
      icon_: {type: String},
      color_: {type: Number},
      colors_: {type: Array},
      pickerOpen_: {type: Boolean},
      added_: {type: Array},
      isDefault_: {type: Boolean},
    };
  }

  protected accessor step_: Step = 'hello';
  protected accessor steps_: Step[] = stepsFor(false);
  protected accessor name_: string = s('welcomePersonal');
  protected accessor icon_: string = '🏠';
  protected accessor color_: number = 0;
  protected accessor colors_: readonly number[] = COLORS;
  protected accessor pickerOpen_: boolean = false;
  protected accessor added_: string[] = [];
  protected accessor isDefault_: boolean = false;
  private readonly handler_ = new PageHandlerRemote();

  override async connectedCallback() {
    super.connectedCallback();
    PageHandlerFactory.getRemote().createPageHandler(
        this.handler_.$.bindNewPipeAndPassReceiver());
    const {state} = await this.handler_.getState();
    this.steps_ = stepsFor(state.arcFound);
    this.isDefault_ = state.isDefaultBrowser;
    // A space with a name of its own keeps it (Chromium's own name for a new
    // profile comes empty).
    if (state.spaceName && state.spaceName !== 'Cromarc') {
      this.name_ = state.spaceName;
    }
  }

  protected colorCss_(color: number): string {
    return cssColor(color);
  }

  protected iconMask_(): string {
    return iconMask(this.icon_);
  }

  protected iconText_(): string {
    return iconMask(this.icon_) ? '' : this.icon_;
  }

  protected onNextClick_() {
    if (this.step_ === 'space') {
      this.handler_.setupSpace(this.name_.trim() || s('welcomePersonal'), this.icon_,
                               this.color_);
    }
    this.step_ = nextStep(this.steps_, this.step_);
  }

  protected onIconClick_() {
    this.pickerOpen_ = !this.pickerOpen_;
  }

  protected onIconPicked_(e: CustomEvent<string>) {
    this.icon_ = e.detail;
    this.pickerOpen_ = false;
  }

  protected onNameInput_(e: Event) {
    this.name_ = (e.target as HTMLInputElement).value;
  }

  protected onColorClick_(e: Event) {
    this.color_ = Number((e.currentTarget as HTMLElement).dataset['color']);
  }

  protected onAddWorkClick_() {
    this.addSpace_(s('welcomeWork'), '💼');
  }

  protected onAddOwnClick_() {
    const input = this.shadowRoot.querySelector<HTMLInputElement>('#own')!;
    const name = input.value.trim();
    if (name) {
      this.addSpace_(name, '');
      input.value = '';
    }
  }

  protected onOwnKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onAddOwnClick_();
    }
  }

  protected onArcClick_() {
    this.handler_.openArcImport();
    this.step_ = nextStep(this.steps_, this.step_);
  }

  protected async onDefaultClick_() {
    const {isDefault} = await this.handler_.setAsDefaultBrowser();
    this.isDefault_ = isDefault;
    if (isDefault) {
      this.step_ = nextStep(this.steps_, this.step_);
    }
  }

  protected onFinishClick_() {
    this.handler_.finish();
  }

  private addSpace_(name: string, icon: string) {
    this.handler_.addSpace(name, icon);
    this.added_ = [...this.added_, name];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-welcome-app': WelcomeAppElement;
  }
}

customElements.define(WelcomeAppElement.is, WelcomeAppElement);
