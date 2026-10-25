// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {LinkRoute, Space} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import type {LibraryElement} from './library.js';
import {getHtml} from './library_routing.html.js';
import {updateFilterHeight} from './library_search.js';
import {libraryStrings} from './library_strings.js';
import {showMenu} from './menu.js';
import {iconMask} from './space_icon.js';
import './library_search_box.js';

// The form's route; `id` '' for one not saved yet, `space` '' for the space
// used last (Zen's most-recent-space).
export interface RouteDraft {
  id: string;
  reference: string;
  match: string;
  space: string;
}

export interface MatchType {
  id: string;
  label: string;
}

const PANEL_FADE_MS = 300;

// zen:library/sections/ZenLibrarySpaceRoutingSection.mjs: the rules that
// send links to a space, a form for one in the filter panel's place, and
// where links from other apps go. Draws into the Library's shadow root.
export class LibraryRoutingElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-routing';
  }

  override createRenderRoot() {
    return this;
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      library: {type: Object},
      spaces: {type: Array},
      routes_: {type: Array},
      defaultExternal_: {type: String},
      draft_: {type: Object},
      formOpen_: {type: Boolean},
      query_: {type: String},
      overflowing_: {type: Boolean, reflect: true, attribute: 'list-overflowing'},
      matchTypes_: {type: Array},
      strings: {type: Object},
    };
  }

  accessor library: LibraryElement|null = null;
  accessor spaces: Space[] = [];
  protected accessor routes_: LinkRoute[] = [];
  protected accessor defaultExternal_: string = '';
  protected accessor draft_: RouteDraft|null = null;
  protected accessor formOpen_: boolean = false;
  protected accessor query_: string = '';
  protected accessor overflowing_: boolean = false;
  protected accessor matchTypes_: MatchType[] = [
    {id: 'contains', label: libraryStrings.routingContains},
    {id: 'equal-to', label: libraryStrings.routingEqualTo},
    {id: 'regex', label: libraryStrings.routingRegex},
  ];
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private draftTimer_ = 0;
  private resizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.load_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    clearTimeout(this.draftTimer_);
    this.resizeObserver_?.disconnect();
  }

  override firstUpdated(changed: PropertyValues<this>) {
    super.firstUpdated(changed);
    const results = this.querySelector<HTMLElement>('.search-results');
    if (results) {
      this.resizeObserver_ = new ResizeObserver(() => this.checkOverflow_());
      this.resizeObserver_.observe(results);
    }
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    this.checkOverflow_();
    if (changed.has('formOpen_' as keyof LibraryRoutingElement) &&
        this.formOpen_) {
      updateFilterHeight(this);
    }
  }

  private checkOverflow_() {
    const results = this.querySelector<HTMLElement>('.search-results');
    if (results) {
      this.overflowing_ = results.scrollHeight > results.clientHeight;
    }
  }

  private async load_() {
    const {routes, defaultExternal} = await this.proxy_.handler.getRoutes();
    this.routes_ = routes;
    this.defaultExternal_ = defaultExternal;
  }

  // From the Library.

  onShown() {
    // Routes can be added from elsewhere while the Library shows another
    // section.
    this.load_();
    this.updateComplete.then(
        () => this.querySelector<HTMLElement>('.search-box')?.focus());
  }

  onLibraryOpened() {
    this.onShown();
  }

  onLibraryClosing() {
    clearTimeout(this.draftTimer_);
    this.formOpen_ = false;
    this.draft_ = null;
  }

  onEscape(): boolean {
    if (!this.formOpen_) {
      return false;
    }
    this.closeForm_();
    return true;
  }

  // Template helpers.

  protected shownRoutes_(): LinkRoute[] {
    const query = this.query_.toLowerCase();
    if (!query) {
      return this.routes_;
    }
    return this.routes_.filter(
        route => route.reference.toLowerCase().includes(query) ||
            this.spaceName_(route.space).toLowerCase().includes(query));
  }

  private space_(id: string): Space|undefined {
    return this.spaces.find(space => space.id === id);
  }

  protected spaceName_(id: string): string {
    return this.space_(id)?.name ?? '';
  }

  protected destinationLabel_(id: string): string {
    return this.spaceName_(id) || this.strings.routingMostRecent;
  }

  // A space is known by its icon, a picture or a glyph; one without, and
  // the space used last, go by name alone.
  protected spaceIconMask_(id: string): string {
    return iconMask(this.space_(id)?.icon ?? '');
  }

  protected spaceGlyph_(id: string): string {
    const icon = this.space_(id)?.icon ?? '';
    return iconMask(icon) ? '' : icon;
  }

  protected matchLabel_(match: string): string {
    return (this.matchTypes_.find(type => type.id === match) ??
            this.matchTypes_[0]!)
        .label;
  }

  protected draftIsValid_(): boolean {
    const reference = this.draft_?.reference.trim();
    if (!reference) {
      return false;
    }
    if (this.draft_!.match !== 'regex') {
      return true;
    }
    try {
      new RegExp(reference);
      return true;
    } catch {
      return false;
    }
  }

  protected referenceInvalid_(): boolean {
    return !!this.draft_?.reference.trim() && !this.draftIsValid_();
  }

  // The form.

  protected onNewClick_() {
    clearTimeout(this.draftTimer_);
    this.draft_ = {id: '', reference: '', match: 'contains', space: ''};
    this.formOpen_ = true;
  }

  protected onRouteClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['id'];
    const route = this.routes_.find(route => route.id === id);
    if (!route) {
      return;
    }
    clearTimeout(this.draftTimer_);
    this.draft_ = {...route};
    this.formOpen_ = true;
  }

  private closeForm_() {
    this.formOpen_ = false;
    this.draftTimer_ = setTimeout(() => {
      if (!this.formOpen_) {
        this.draft_ = null;
      }
    }, PANEL_FADE_MS);
  }

  protected onCancelClick_() {
    this.closeForm_();
  }

  protected onMatchClick_(e: Event) {
    const match = (e.currentTarget as HTMLElement).dataset['match'];
    if (this.draft_ && match) {
      this.draft_ = {...this.draft_, match};
    }
  }

  protected onReferenceInput_(e: Event) {
    if (this.draft_) {
      this.draft_ = {
        ...this.draft_,
        reference: (e.target as HTMLInputElement).value,
      };
    }
  }

  protected onReferenceKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onSaveClick_();
    }
  }

  protected async onSaveClick_() {
    if (!this.draft_ || !this.draftIsValid_()) {
      return;
    }
    const {id, match, space} = this.draft_;
    const reference = this.draft_.reference.trim();
    if (id) {
      this.proxy_.handler.updateRoute(id, reference, match, space);
    } else {
      this.proxy_.handler.addRoute(reference, match, space);
    }
    this.closeForm_();
    await this.load_();
  }

  protected async onRemoveClick_(e: Event) {
    e.stopPropagation();
    const id = (e.currentTarget as HTMLElement).dataset['id'];
    if (!id) {
      return;
    }
    this.proxy_.handler.removeRoute(id);
    if (this.draft_?.id === id) {
      this.closeForm_();
    }
    await this.load_();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.query_ = e.detail;
  }

  // Both pickers open a native menu of the spaces, the space used last first.
  private pick_(e: MouseEvent, picked: string, onPick: (id: string) => void) {
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
    showMenu(rect.left, rect.bottom, [
      {
        label: this.strings.routingMostRecent,
        checked: !this.space_(picked),
        action: () => onPick(''),
      },
      ...this.spaces.map(space => ({
                           label: `${
                               iconMask(space.icon) || !space.icon ?
                                   '' :
                                   `${space.icon} `}${space.name}`,
                           checked: space.id === picked,
                           action: () => onPick(space.id),
                         })),
    ]);
  }

  protected onDraftSpaceClick_(e: MouseEvent) {
    if (this.draft_) {
      this.pick_(e, this.draft_.space, space => {
        if (this.draft_) {
          this.draft_ = {...this.draft_, space};
        }
      });
    }
  }

  protected onExternalSpaceClick_(e: MouseEvent) {
    this.pick_(e, this.defaultExternal_, space => {
      this.proxy_.handler.setDefaultRoute(space);
      this.defaultExternal_ = space;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-routing': LibraryRoutingElement;
  }
}

customElements.define(LibraryRoutingElement.is, LibraryRoutingElement);
