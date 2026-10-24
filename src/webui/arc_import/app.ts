// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {NEW_SPACE, PageHandlerFactory, PageHandlerRemote} from '../arc_import.mojom-webui.js';
import type {ArcSpace, Target} from '../arc_import.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {defaultTargets} from './choices.js';

// Arc's sidebar into Cromarc spaces (design, UI section 12, step 3): each
// Arc space goes to an existing space, a new one, or nowhere. Arc's own data
// comes first; its export file is for when that is gone.
export class ArcImportAppElement extends CrLitElement {
  static get is() {
    return 'cromarc-arc-import-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      loading_: {type: Boolean},
      found_: {type: Boolean},
      fromExport_: {type: Boolean},
      exportFailed_: {type: Boolean},
      spaces_: {type: Array},
      targets_: {type: Array},
      choices_: {type: Object},
      importing_: {type: Boolean},
      summary_: {type: String},
      newSpace_: {type: String},
    };
  }

  protected accessor loading_: boolean = true;
  protected accessor found_: boolean = false;
  // The spaces come from Arc's export file, not from Arc's data.
  protected accessor fromExport_: boolean = false;
  protected accessor exportFailed_: boolean = false;
  protected accessor spaces_: ArcSpace[] = [];
  protected accessor targets_: Target[] = [];
  protected accessor choices_: Record<string, string> = {};
  protected accessor importing_: boolean = false;
  protected accessor summary_: string = '';
  protected accessor newSpace_: string = NEW_SPACE;

  private readonly handler_ = new PageHandlerRemote();

  override async connectedCallback() {
    super.connectedCallback();
    PageHandlerFactory.getRemote().createPageHandler(
        this.handler_.$.bindNewPipeAndPassReceiver());
    const {found, spaces, targets} = await this.handler_.getArcSidebar();
    this.show_(found, spaces, targets);
    this.loading_ = false;
  }

  private show_(found: boolean, spaces: ArcSpace[], targets: Target[]) {
    this.found_ = found;
    this.spaces_ = spaces;
    this.targets_ = targets;
    this.choices_ = defaultTargets(spaces, targets, NEW_SPACE);
    this.summary_ = '';
  }

  protected async onChooseExportClick_() {
    this.exportFailed_ = false;
    const {found, spaces, targets} = await this.handler_.chooseExportFile();
    if (!found) {
      this.exportFailed_ = true;
      return;
    }
    this.fromExport_ = true;
    this.show_(found, spaces, targets);
  }

  protected countsOf_(space: ArcSpace): string {
    const counts = [`${space.folders} папок`, `${space.pinned} закреплённых`];
    if (!this.fromExport_) {
      counts.push(`${space.essentials} важных`, `${space.today} в Today`);
    }
    return counts.join(' · ');
  }

  protected iconOf_(space: ArcSpace): string {
    return space.icon || space.title.charAt(0).toUpperCase();
  }

  protected targetLabel_(target: Target): string {
    return `${target.icon} ${target.name}${target.current ? ' (этот)' : ''}`;
  }

  protected isChosen_(arcId: string, target: string): boolean {
    return this.choices_[arcId] === target;
  }

  protected onTargetChange_(e: Event) {
    const select = e.target as HTMLSelectElement;
    this.choices_ = {...this.choices_, [select.dataset['arcId']!]: select.value};
  }

  protected async onImportClick_() {
    this.importing_ = true;
    const choices = Object.entries(this.choices_).map(
        ([arcId, target]) => ({arcId, target}));
    const {summary} = await this.handler_.import(choices);
    this.summary_ = summary || 'Нечего переносить.';
    this.importing_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-arc-import-app': ArcImportAppElement;
  }
}

customElements.define(ArcImportAppElement.is, ArcImportAppElement);
