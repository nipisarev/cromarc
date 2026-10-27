// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, ResultKind} from '../command_bar.mojom-webui.js';
import type {Result} from '../command_bar.mojom-webui.js';
import {s} from '../i18n/i18n.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

const KIND_LABELS: Record<ResultKind, string> = {
  [ResultKind.kTab]: s('commandBarKindTab'),
  [ResultKind.kPinned]: s('commandBarKindPinned'),
  [ResultKind.kSuggestion]: '',
  [ResultKind.kArchive]: s('commandBarKindArchive'),
};

// The command bar (design, component 6): one field, results from the window's
// tabs, the space's pinned items, the omnibox and the archive.
export class CommandBarAppElement extends CrLitElement {
  static get is() {
    return 'cromarc-command-bar-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      text_: {type: String},
      results_: {type: Array},
      selected_: {type: Number},
    };
  }

  protected accessor text_: string = '';
  protected accessor results_: Result[] = [];
  protected accessor selected_: number = 0;

  private readonly handler_ = new PageHandlerRemote();
  private readonly router_ = new PageCallbackRouter();
  private selectAllOnShow_ = false;

  override connectedCallback() {
    super.connectedCallback();
    this.router_.onShown.addListener((text: string) => this.onShown_(text));
    this.router_.onResults.addListener(
        (text: string, results: Result[]) => this.onResults_(text, results));
    PageHandlerFactory.getRemote().createPageHandler(
        this.router_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  private onShown_(text: string) {
    this.text_ = text;
    this.results_ = [];
    this.selected_ = 0;
    this.selectAllOnShow_ = true;
    this.requestUpdate();
  }

  private onResults_(text: string, results: Result[]) {
    if (text !== this.text_) {
      return;
    }
    this.results_ = results;
    this.selected_ = Math.min(this.selected_, Math.max(0, results.length - 1));
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (this.selectAllOnShow_) {
      this.selectAllOnShow_ = false;
      const input = this.shadowRoot.querySelector<HTMLInputElement>('#input');
      input?.focus();
      input?.select();
    }
  }

  // Template helpers.

  protected iconFor_(result: Result): string {
    return getFaviconForPageURL(result.url, false);
  }

  protected labelFor_(result: Result): string {
    return KIND_LABELS[result.kind];
  }

  protected isSelected_(index: number): boolean {
    return index === this.selected_;
  }

  // Events.

  protected onInputInput_(e: Event) {
    this.text_ = (e.target as HTMLInputElement).value;
    this.selected_ = 0;
    this.handler_.query(this.text_);
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        this.selected_ = Math.min(this.selected_ + 1, this.results_.length - 1);
        break;
      case 'ArrowUp':
        e.preventDefault();
        this.selected_ = Math.max(this.selected_ - 1, 0);
        break;
      case 'Enter': {
        e.preventDefault();
        const result = this.results_[this.selected_];
        if (result) {
          this.handler_.open(result);
        }
        break;
      }
      case 'Escape':
        e.preventDefault();
        this.handler_.close();
        break;
    }
  }

  protected onResultClick_(e: Event) {
    const index =
        Number((e.currentTarget as HTMLElement).dataset['index'] ?? '-1');
    const result = this.results_[index];
    if (result) {
      this.handler_.open(result);
    }
  }

  protected onResultMouseenter_(e: Event) {
    this.selected_ =
        Number((e.currentTarget as HTMLElement).dataset['index'] ?? '0');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-command-bar-app': CommandBarAppElement;
  }
}

customElements.define(CommandBarAppElement.is, CommandBarAppElement);
