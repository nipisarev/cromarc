// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {Space} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {isPending, progressOf} from './downloads_model.js';
import {downloadsStore} from './downloads_store.js';
import {getCss} from './library.css.js';
import {getHtml} from './library.html.js';
import {libraryStrings} from './library_strings.js';
import {MEDIA_ENABLED_KEY, mediaEnabled} from './library_media.js';
import type {LibraryMediaElement} from './library_media.js';
import {spriteUrl} from './library_sprites.js';
import {showMenu} from './menu.js';
import type {MenuItem} from './menu.js';
import './library_downloads.js';
import './library_history.js';
import './library_media.js';
import './library_routing.js';
import './library_spaces.js';

export interface LibraryTab {
  id: string;
  label: string;
}

// What a section of the Library may want to hear (zen:library/ZenLibrary.mjs
// calls them on the section element).
export interface LibrarySection extends HTMLElement {
  onShown?(): void;
  onEscape?(): boolean;
  onLibraryOpened?(): void;
  onLibraryClosing?(): void;
}

export interface LibraryConfirm {
  title: string;
  message: string;
  accept: string;
  resolve: (accepted: boolean) => void;
}

// zen:library/ZenLibrary.mjs zenLibrarySections, in its order; the ones not
// here yet are still to come over.
const SECTIONS: LibraryTab[] = [
  {id: 'space-routing', label: libraryStrings.routing},
  {id: 'media', label: libraryStrings.media},
  {id: 'downloads', label: libraryStrings.downloads},
  {id: 'spaces', label: libraryStrings.spaces},
  {id: 'history', label: libraryStrings.history},
];
const LAST_TAB_KEY = 'cromarc.library.lastTab';
const HIDDEN_TABS_KEY = 'cromarc.library.hiddenTabs';
const CLOSED_TABS_KEY = 'cromarc.library.closedTabs';
const TOAST_MS = 3000;
// Clear history (ZenLibraryHistorySection's CLEAR_RANGES).
const HOUR = 3600000;
interface ClearRange {
  label: string;
  hours: number;
  sinceMidnight?: boolean;
}
const CLEAR_RANGES: ClearRange[] = [
  {label: libraryStrings.clearLastHour, hours: 1},
  {label: libraryStrings.clearLast12Hours, hours: 12},
  {label: libraryStrings.clearToday, hours: 0, sinceMidnight: true},
  {label: libraryStrings.clearLastWeek, hours: 24 * 7},
  {label: libraryStrings.clearLastMonth, hours: 24 * 30},
  {label: '', hours: 0},
  {label: libraryStrings.clearAll, hours: 0},
];

function readStorage(key: string): string|null {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function writeStorage(key: string, value: string) {
  try {
    localStorage.setItem(key, value);
  } catch {
    // Not kept: the Library starts as it would the first time.
  }
}

// The Library (zen:library/ZenLibrary.mjs): a rail of sections down the side,
// the chosen one beside it. Its sections draw into this element's shadow
// root, styled by library.css as Zen styles them all from zen-library.css.
// Fires `library-close`.
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
      spaces: {type: Array},
      open: {type: Boolean, reflect: true},
      primary: {type: String},
      activeTab_: {type: String},
      customizing_: {type: Boolean},
      hiddenTabs_: {type: Array},
      mounted_: {type: Array},
      animatingTab_: {type: String},
      closedTabs_: {type: Boolean},
      toast_: {type: String},
      confirm_: {type: Object},
      spriteColors_: {type: Object},
      downloading_: {type: Boolean},
      strings: {type: Object},
    };
  }

  accessor spaces: Space[] = [];
  accessor open: boolean = false;
  accessor primary: string = '';
  protected accessor activeTab_: string = 'history';
  protected accessor customizing_: boolean = false;
  protected accessor hiddenTabs_: string[] = [];
  protected accessor mounted_: string[] = [];
  protected accessor animatingTab_: string = '';
  // The history section lists the archive: its tab reads "Архив".
  protected accessor closedTabs_: boolean = false;
  protected accessor toast_: string = '';
  protected accessor confirm_: LibraryConfirm|null = null;
  protected accessor spriteColors_ = {stroke: '', fill: ''};
  // The newest download is coming in: its tab fills and rings (Zen's
  // zen-library-downloading, from ZenLibraryWidget).
  protected accessor downloading_: boolean = false;
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private toastTimer_ = 0;
  private readonly scheme_ = window.matchMedia('(prefers-color-scheme: dark)');
  private readonly onSchemeChange_ = () => this.readSpriteColors_();
  private readonly onDocumentKeydown_ = (e: KeyboardEvent) =>
      this.onEscapeKey_(e);
  private unsubscribe_ = () => {};
  // Sections that asked for a sprite before the colours were read.
  private spriteWaiters_: Array<() => void> = [];

  constructor() {
    super();
    const last = readStorage(LAST_TAB_KEY) ?? 'history';
    this.activeTab_ =
        SECTIONS.some(section => section.id === last) ? last : 'history';
    this.hiddenTabs_ = (readStorage(HIDDEN_TABS_KEY) ?? '')
                           .split(',')
                           .filter(id => SECTIONS.some(s => s.id === id));
    this.closedTabs_ = readStorage(CLOSED_TABS_KEY) === '1';
    this.mounted_ = [this.activeTab_];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.scheme_.addEventListener('change', this.onSchemeChange_);
    document.addEventListener('keydown', this.onDocumentKeydown_);
    this.unsubscribe_ = downloadsStore().subscribe(downloads => {
      const newest = downloads[downloads.length - 1];
      this.downloading_ = !!newest && isPending(newest);
      this.style.setProperty(
          '--download-progress',
          `${newest ? Math.round(progressOf(newest)) : 0}%`);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.scheme_.removeEventListener('change', this.onSchemeChange_);
    document.removeEventListener('keydown', this.onDocumentKeydown_);
    clearTimeout(this.toastTimer_);
    this.unsubscribe_();
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('primary') || !this.spriteColors_.stroke) {
      this.readSpriteColors_();
    }
    if (changed.has('activeTab_' as keyof LibraryElement)) {
      // Only Spaces asks for more width (ZenLibrarySpacesSection onHidden
      // takes it back).
      if (this.activeTab_ !== 'spaces') {
        this.requestWidth(0);
      }
      this.section_(this.activeTab_)?.onShown?.();
    }
    if (changed.has('open')) {
      const section = this.section_(this.activeTab_);
      this.open ? section?.onLibraryOpened?.() :
                  section?.onLibraryClosing?.();
      if (!this.open) {
        this.customizing_ = false;
      }
    }
  }

  // Zen's sprite colours: the accent button colour, filled with a lighter
  // (or darker) mix of it on the chosen tab.
  private readSpriteColors_() {
    const probe = this.shadowRoot?.querySelector<HTMLElement>('#sprite-probe');
    if (!probe) {
      return;
    }
    const style = getComputedStyle(probe);
    const colors = {stroke: style.color, fill: style.backgroundColor};
    if (colors.stroke !== this.spriteColors_.stroke ||
        colors.fill !== this.spriteColors_.fill) {
      this.spriteColors_ = colors;
      const waiters = this.spriteWaiters_;
      this.spriteWaiters_ = [];
      waiters.forEach(waiter => waiter());
    }
  }

  private section_(id: string): LibrarySection|null {
    return this.shadowRoot?.querySelector<LibrarySection>(
               `.section[data-section="${id}"]`) ??
        null;
  }

  // Template helpers.

  protected railTabs_(): LibraryTab[] {
    return SECTIONS.filter(
        section => this.customizing_ || !this.hiddenTabs_.includes(section.id));
  }

  protected tabLabel_(tab: LibraryTab): string {
    return tab.id === 'history' && this.closedTabs_ ? this.strings.archive :
                                                      tab.label;
  }

  protected isHidden_(id: string): boolean {
    return this.hiddenTabs_.includes(id);
  }

  protected isMounted_(id: string): boolean {
    return this.mounted_.includes(id);
  }

  protected spriteFor_(id: string): string {
    if (!this.spriteColors_.stroke) {
      return '';
    }
    const active = id === this.activeTab_ ||
        (id === 'downloads' && this.downloading_);
    return spriteUrl(
        id, this.spriteColors_.stroke,
        active ? this.spriteColors_.fill : 'transparent',
        () => this.requestUpdate());
  }

  // The rail.

  protected onTabClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['section'];
    if (!id || this.customizing_ || id === this.activeTab_) {
      return;
    }
    this.select_(id);
    this.animatingTab_ = id;
  }

  private select_(id: string) {
    this.activeTab_ = id;
    if (!this.mounted_.includes(id)) {
      this.mounted_ = [...this.mounted_, id];
    }
    writeStorage(LAST_TAB_KEY, id);
  }

  protected onTabToggleClick_(e: Event) {
    e.stopPropagation();
    const id = (e.currentTarget as HTMLElement).dataset['section'];
    if (!id) {
      return;
    }
    let hidden = this.hiddenTabs_;
    if (hidden.includes(id)) {
      hidden = hidden.filter(other => other !== id);
    } else {
      // The rail is of no use with nothing left in it.
      if (SECTIONS.length - hidden.length < 2) {
        return;
      }
      hidden = [...hidden, id];
    }
    this.hiddenTabs_ = hidden;
    writeStorage(HIDDEN_TABS_KEY, hidden.join(','));
    if (hidden.includes(this.activeTab_)) {
      const next = SECTIONS.find(section => !hidden.includes(section.id));
      if (next) {
        this.select_(next.id);
      }
    }
  }

  // Right clicking a tab offers what its section has for all it lists, and
  // the way to the page it stands in for (Zen: tabMenu, legacyLibraryView).
  protected onTabContextmenu_(e: MouseEvent) {
    const id = (e.currentTarget as HTMLElement).dataset['section'];
    if (id === 'media') {
      if (mediaEnabled()) {
        e.preventDefault();
        showMenu(e.clientX, e.clientY, [{
                   label: this.strings.mediaTurnOff,
                   action: () => this.turnOffMedia_(),
                 }]);
      }
      return;
    }
    if (id === 'downloads') {
      e.preventDefault();
      showMenu(e.clientX, e.clientY, [
        {
          label: this.strings.downloadsClearAll,
          action: () => this.proxy_.handler.clearDownloads(),
        },
        {label: ''},
        {
          label: this.strings.openLegacyDownloads,
          action: () => this.openPage('chrome://downloads/'),
        },
      ]);
      return;
    }
    if (id !== 'history') {
      return;
    }
    e.preventDefault();
    const items: MenuItem[] = [
      {
        label: this.strings.historyClear,
        items: CLEAR_RANGES.map(range => ({
                                  label: range.label,
                                  action: () => this.clearHistory_(range),
                                })),
      },
      {label: ''},
      {
        label: this.strings.openLegacyHistory,
        action: () => this.openPage('chrome://history/'),
      },
    ];
    showMenu(e.clientX, e.clientY, items);
  }

  private turnOffMedia_() {
    const media = this.section_('media') as LibraryMediaElement | null;
    if (media) {
      media.turnOff();
    } else {
      writeStorage(MEDIA_ENABLED_KEY, '0');
    }
  }

  private async clearHistory_(range: ClearRange) {
    const accepted = await this.confirm(
        this.strings.clearPromptTitle, this.strings.clearPromptMessage,
        this.strings.clearPromptAccept);
    if (!accepted) {
      return;
    }
    let begin = 0;
    if (range.sinceMidnight) {
      const midnight = new Date();
      midnight.setHours(0, 0, 0, 0);
      begin = midnight.getTime();
    } else if (range.hours) {
      begin = Date.now() - range.hours * HOUR;
    }
    this.proxy_.handler.clearHistory(begin);
  }

  // The footer.

  protected onCloseClick_() {
    this.fire('library-close');
  }

  protected onCustomizeClick_() {
    this.customizing_ = !this.customizing_;
  }

  // For the sections.

  // A section's sprite as its tab shows it chosen; `onLoad` once it has
  // loaded, if it had not.
  spriteImage(id: string, onLoad: () => void): string {
    if (!this.spriteColors_.stroke) {
      this.spriteWaiters_.push(onLoad);
      return '';
    }
    return spriteUrl(
        id, this.spriteColors_.stroke, this.spriteColors_.fill, onLoad);
  }

  // Opens `url` in a tab and closes the Library; with `background` the tab
  // stays behind and the Library open, saying so.
  openPage(url: string, background = false) {
    if (background) {
      this.fire('library-keep-open');
    }
    this.proxy_.handler.openUrl(url, background);
    if (background) {
      this.showToast(this.strings.openedInBackground);
    } else {
      this.fire('library-close');
    }
  }

  // Zen's --zen-library-content-width: 0 for the Library's own 418px.
  requestWidth(width: number) {
    this.fire('library-width', width);
  }

  showToast(text: string) {
    clearTimeout(this.toastTimer_);
    this.toast_ = text;
    this.toastTimer_ = setTimeout(() => this.toast_ = '', TOAST_MS);
  }

  // Zen asks with Services.prompt.confirmEx; here the question comes up over
  // the Library.
  confirm(title: string, message: string, accept: string): Promise<boolean> {
    this.confirm_?.resolve(false);
    return new Promise(resolve => {
      this.confirm_ = {title, message, accept, resolve};
    });
  }

  protected onConfirmAcceptClick_() {
    this.confirm_?.resolve(true);
    this.confirm_ = null;
  }

  protected onConfirmCancelClick_() {
    this.confirm_?.resolve(false);
    this.confirm_ = null;
  }

  // The history section switched between history and the archive.
  protected onClosedTabsChanged_(e: CustomEvent<boolean>) {
    this.closedTabs_ = e.detail;
    writeStorage(CLOSED_TABS_KEY, e.detail ? '1' : '0');
  }

  closedTabs(): boolean {
    return this.closedTabs_;
  }

  // ZenLibrary.onKeyDown: Escape first goes to the section (an open filter
  // panel), then shuts the Library, unless the keyboard is busy in it
  // outside its search box.
  private onEscapeKey_(e: KeyboardEvent) {
    if (!this.open || e.key !== 'Escape') {
      return;
    }
    if (this.confirm_) {
      this.onConfirmCancelClick_();
      e.preventDefault();
      return;
    }
    if (this.section_(this.activeTab_)?.onEscape?.()) {
      e.preventDefault();
      return;
    }
    const focused = this.shadowRoot?.activeElement;
    if (focused && !focused.closest('.search-box')) {
      return;
    }
    this.fire('library-close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library': LibraryElement;
  }
}

customElements.define(LibraryElement.is, LibraryElement);
