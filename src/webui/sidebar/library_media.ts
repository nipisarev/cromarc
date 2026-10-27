// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {cachedFormat, uiLocale} from '../i18n/i18n.js';
import {MediaCommand} from '../sidebar.mojom-webui.js';
import type {MediaItem} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import type {LibraryElement} from './library.js';
import type {FilterToggle} from './library_filters.js';
import {getHtml} from './library_media.html.js';
import {MS_PER_DAY} from './library_model.js';
import {filterKey, toggleFilter, updateFilterHeight} from './library_search.js';
import type {FilterGroup} from './library_search.js';
import {libraryStrings} from './library_strings.js';
import {showMenu} from './menu.js';
import './library_filters.js';
import './library_search_box.js';

// zen.library.media.enabled and zen.library.media.folders, per profile.
export const MEDIA_ENABLED_KEY = 'cromarc.library.media.enabled';
const FOLDERS_KEY = 'cromarc.library.media.folders';
const DEFAULT_FOLDERS = 'downloads,documents,pictures';
// The preview takes as much of the window as the column can have.
const PREVIEW_WIDTH = 4000;

const relativeTime = () => cachedFormat(
    'relativeTime', () => new Intl.RelativeTimeFormat(uiLocale(), {numeric: 'auto'}));

function readStorage(key: string): string|null {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

export function mediaEnabled(): boolean {
  return readStorage(MEDIA_ENABLED_KEY) === '1';
}

// zen:library/sections/ZenLibraryMediaSection.mjs, with its preview
// (ZenLibraryMediaPreview.mjs): pictures and videos from a few folders, once
// agreed to, newest first; a click shows one big, with the others a step
// away. Draws into the Library's shadow root.
export class LibraryMediaElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-media';
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
      enabled_: {type: Boolean},
      loading_: {type: Boolean},
      items_: {type: Array},
      roots_: {type: Array},
      filtersOpen_: {type: Boolean},
      active_: {type: Array},
      query_: {type: String},
      preview_: {type: Number},
      opener_: {type: String},
      strings: {type: Object},
    };
  }

  accessor library: LibraryElement|null = null;
  protected accessor enabled_: boolean = mediaEnabled();
  protected accessor loading_: boolean = false;
  protected accessor items_: MediaItem[] = [];
  protected accessor roots_: string[] = [];
  protected accessor filtersOpen_: boolean = false;
  protected accessor active_: string[] = [];
  protected accessor query_: string = '';
  // The item shown big, by its place in the grid; -1 for none.
  protected accessor preview_: number = -1;
  protected accessor opener_: string = '';
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private scan_ = 0;
  private readonly onKeydown_ = (e: KeyboardEvent) => this.onPreviewKey_(e);

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('keydown', this.onKeydown_);
    if (this.enabled_) {
      this.scan();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('keydown', this.onKeydown_);
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('filtersOpen_' as keyof LibraryMediaElement) &&
        this.filtersOpen_) {
      updateFilterHeight(this);
    }
  }

  // From the Library.

  onShown() {
    this.updateComplete.then(
        () => this.querySelector<HTMLElement>('.search-box')?.focus());
  }

  onLibraryOpened() {
    this.onShown();
  }

  onLibraryClosing() {
    this.closePreview_();
  }

  onEscape(): boolean {
    if (this.preview_ >= 0) {
      this.closePreview_();
      return true;
    }
    if (this.filtersOpen_) {
      this.filtersOpen_ = false;
      return true;
    }
    return false;
  }

  // From the tab's menu (Zen's "Turn off Media").
  turnOff() {
    try {
      localStorage.setItem(MEDIA_ENABLED_KEY, '0');
    } catch {
      // Not kept: it asks again next time.
    }
    this.scan_++;
    this.closePreview_();
    this.enabled_ = false;
    this.items_ = [];
    this.roots_ = [];
    this.loading_ = false;
  }

  // Reading.

  protected onEnableClick_() {
    try {
      localStorage.setItem(MEDIA_ENABLED_KEY, '1');
    } catch {
      // Not kept: it asks again next time.
    }
    this.enabled_ = true;
    this.scan();
  }

  async scan() {
    const scan = ++this.scan_;
    this.loading_ = true;
    const folders = (readStorage(FOLDERS_KEY) ?? DEFAULT_FOLDERS)
                        .split(',')
                        .map(folder => folder.trim())
                        .filter(Boolean);
    const {roots, items} = await this.proxy_.handler.scanMedia(folders);
    if (scan !== this.scan_) {
      return;
    }
    this.roots_ = roots;
    this.items_ = items;
    this.loading_ = false;
  }

  // Filters.

  protected filterGroups_(): FilterGroup[] {
    return [{
      id: 'folder',
      title: this.strings.mediaFilterFolders,
      options: this.roots_.map(root => ({
                                 id: root,
                                 label: root.split('/').pop() ?? root,
                               })),
    }];
  }

  protected hasFilters_(): boolean {
    return this.enabled_ && this.roots_.length >= 2;
  }

  protected onFilterButtonClick_() {
    this.filtersOpen_ = true;
  }

  protected onFilterDoneClick_() {
    this.filtersOpen_ = false;
  }

  protected onFilterToggled_(e: CustomEvent<FilterToggle>) {
    this.active_ = toggleFilter(this.active_, e.detail.group, e.detail.option);
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.query_ = e.detail;
  }

  // What the grid shows (ZenLibraryMediaSection #shown).
  protected shown_(): MediaItem[] {
    const folders = this.roots_.filter(
        root => this.active_.includes(filterKey('folder', root)));
    const query = this.query_.toLowerCase();
    return this.items_.filter(
        item => (!folders.length || folders.includes(item.folder)) &&
            (!query || item.name.toLowerCase().includes(query)));
  }

  // Template helpers.

  protected thumbSrc_(item: MediaItem): string {
    // A file iCloud has not brought down is not read for a picture.
    return item.cloudPlaceholder ?
        `chrome://fileicon/?path=${encodeURIComponent(item.name)}&scale=2x` :
        `media-thumb/${item.id}.png`;
  }

  protected previewItem_(): MediaItem|null {
    return this.shown_()[this.preview_] ?? null;
  }

  protected previewIsVideo_(): boolean {
    const item = this.previewItem_();
    return !!item && item.kind === 'video' && item.servedWhole &&
        !item.cloudPlaceholder;
  }

  protected previewSrc_(): string {
    const item = this.previewItem_();
    if (!item) {
      return '';
    }
    if (item.cloudPlaceholder) {
      return this.thumbSrc_(item);
    }
    if (item.kind === 'video') {
      return item.servedWhole ? `media-file/${item.id}.mp4` :
                                `media-large/${item.id}.png`;
    }
    // What the page cannot draw shows QuickLook's picture of it.
    return item.kind === 'image' && item.servedWhole ?
        `media-file/${item.id}.png` :
        `media-large/${item.id}.png`;
  }

  // ZenLibraryMediaPreview #relativeDate: days, weeks past a week.
  protected previewDate_(): string {
    const item = this.previewItem_();
    if (!item) {
      return '';
    }
    const days = Math.round((item.modified - Date.now()) / MS_PER_DAY);
    return Math.abs(days) >= 7 ?
        relativeTime().format(Math.round(days / 7), 'week') :
        relativeTime().format(days, 'day');
  }

  // Zen's opt-in shows the section's own sprite.
  protected optInSprite_(): string {
    return this.library?.spriteImage('media', () => this.requestUpdate()) ??
        '';
  }

  protected openLabel_(): string {
    return this.strings.mediaOpenIn(this.opener_);
  }

  // The grid.

  private itemOf_(e: Event): MediaItem|undefined {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    return this.items_.find(item => item.id === id);
  }

  protected onItemClick_(e: Event) {
    const item = this.itemOf_(e);
    if (item) {
      this.openPreview_(this.shown_().indexOf(item));
    }
  }

  protected onItemContextmenu_(e: MouseEvent) {
    e.preventDefault();
    const item = this.itemOf_(e);
    if (item) {
      this.openMenu_(item, e.clientX, e.clientY);
    }
  }

  private openMenu_(item: MediaItem, x: number, y: number) {
    showMenu(x, y, [
      {
        label: this.strings.mediaMenuOpen,
        action: () => this.proxy_.handler.doMedia(item.id, MediaCommand.kOpen),
      },
      {
        label: this.strings.showInFinder,
        action: () => this.proxy_.handler.doMedia(item.id, MediaCommand.kShow),
      },
      {label: ''},
      {
        label: this.strings.mediaMenuCopy,
        action: () => this.proxy_.handler.doMedia(item.id, MediaCommand.kCopy),
      },
    ]);
  }

  // The preview.

  private openPreview_(index: number) {
    if (index < 0) {
      return;
    }
    this.preview_ = index;
    this.library?.requestWidth(PREVIEW_WIDTH);
    this.updateOpener_();
  }

  private async updateOpener_() {
    const item = this.previewItem_();
    this.opener_ = '';
    if (item) {
      const {app} = await this.proxy_.handler.getMediaOpener(item.id);
      if (this.previewItem_() === item) {
        this.opener_ = app;
      }
    }
  }

  private closePreview_() {
    if (this.preview_ < 0) {
      return;
    }
    this.preview_ = -1;
    this.library?.requestWidth(0);
  }

  private step_(delta: number) {
    const next = this.preview_ + delta;
    if (this.preview_ >= 0 && next >= 0 && next < this.shown_().length) {
      this.preview_ = next;
      this.updateOpener_();
    }
  }

  protected onPreviewCloseClick_() {
    this.closePreview_();
  }

  protected onPreviewBackdropClick_() {
    this.closePreview_();
  }

  protected onPreviewPrevClick_() {
    this.step_(-1);
  }

  protected onPreviewNextClick_() {
    this.step_(1);
  }

  protected onPreviewOpenClick_() {
    const item = this.previewItem_();
    if (item) {
      this.proxy_.handler.doMedia(item.id, MediaCommand.kOpen);
    }
  }

  protected onPreviewMoreClick_(e: MouseEvent) {
    const item = this.previewItem_();
    if (item) {
      const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
      this.openMenu_(item, rect.left, rect.bottom + 4);
    }
  }

  protected onStageContextmenu_(e: MouseEvent) {
    e.preventDefault();
    const item = this.previewItem_();
    if (item) {
      this.openMenu_(item, e.clientX, e.clientY);
    }
  }

  // ZenLibraryMediaPreview #handleKey: arrows step, Escape is the Library's.
  private onPreviewKey_(e: KeyboardEvent) {
    if (this.preview_ < 0) {
      return;
    }
    if (e.key === 'ArrowLeft') {
      e.preventDefault();
      this.step_(-1);
    } else if (e.key === 'ArrowRight') {
      e.preventDefault();
      this.step_(1);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-media': LibraryMediaElement;
  }
}

customElements.define(LibraryMediaElement.is, LibraryMediaElement);
