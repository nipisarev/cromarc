// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DownloadCommand} from '../sidebar.mojom-webui.js';
import type {Download} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {canRetry, FILE_GROUPS, fileGroupOf, hasFile, hasProgress, isPending, progressOf, statusText} from './downloads_model.js';
import {downloadsStore} from './downloads_store.js';
import type {LibraryElement} from './library.js';
import {getHtml} from './library_downloads.html.js';
import type {FilterToggle} from './library_filters.js';
import {formatUrl, MS_PER_DAY, PAGE_SIZE, WHEN_DAYS} from './library_model.js';
import {filterKey, toggleFilter, updateFilterHeight, watchResults} from './library_search.js';
import type {FilterGroup} from './library_search.js';
import {libraryStrings} from './library_strings.js';
import {showMenu} from './menu.js';
import type {MenuItem} from './menu.js';
import './library_filters.js';
import './library_search_box.js';

const TYPE_LABELS: Record<string, string> = {
  images: libraryStrings.typeImages,
  video: libraryStrings.typeVideo,
  audio: libraryStrings.typeAudio,
  documents: libraryStrings.typeDocuments,
  archives: libraryStrings.typeArchives,
  apps: libraryStrings.typeApps,
};

// zen:library/sections/ZenLibraryDownloadsSection.mjs: the profile's
// downloads, newest first, by type and time, each with what can be done to
// it. Draws into the Library's shadow root.
export class LibraryDownloadsElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-downloads';
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
      downloads_: {type: Array},
      loaded_: {type: Boolean},
      filtersOpen_: {type: Boolean},
      active_: {type: Array},
      query_: {type: String},
      limit_: {type: Number},
      menuRow_: {type: Number},
      strings: {type: Object},
    };
  }

  accessor library: LibraryElement|null = null;
  protected accessor downloads_: Download[] = [];
  protected accessor loaded_: boolean = false;
  protected accessor filtersOpen_: boolean = false;
  protected accessor active_: string[] = [];
  protected accessor query_: string = '';
  protected accessor limit_: number = PAGE_SIZE;
  protected accessor menuRow_: number = -1;
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private unsubscribe_ = () => {};
  private stopWatching_ = () => {};

  override connectedCallback() {
    super.connectedCallback();
    this.unsubscribe_ = downloadsStore().subscribe(downloads => {
      this.downloads_ = downloads;
      this.loaded_ = true;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.unsubscribe_();
    this.stopWatching_();
  }

  override firstUpdated(changed: PropertyValues<this>) {
    super.firstUpdated(changed);
    this.stopWatching_ = watchResults(this, () => {
      if (this.visible_().length >= this.limit_) {
        this.limit_ += PAGE_SIZE;
      }
    });
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('filtersOpen_' as keyof LibraryDownloadsElement) &&
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

  onEscape(): boolean {
    if (!this.filtersOpen_) {
      return false;
    }
    this.filtersOpen_ = false;
    return true;
  }

  // Filters.

  protected filterGroups_(): FilterGroup[] {
    const s = this.strings;
    return [
      {
        id: 'type',
        title: s.downloadsFilterType,
        options: FILE_GROUPS.map(id => ({id, label: TYPE_LABELS[id]!})),
      },
      {
        id: 'when',
        title: s.downloadsFilterWhen,
        exclusive: true,
        options: [
          {id: 'today', label: s.today},
          {id: 'week', label: s.week},
          {id: 'month', label: s.month},
        ],
      },
    ];
  }

  protected onFilterButtonClick_() {
    this.filtersOpen_ = true;
  }

  protected onFilterDoneClick_() {
    this.filtersOpen_ = false;
  }

  protected onFilterToggled_(e: CustomEvent<FilterToggle>) {
    this.active_ = toggleFilter(this.active_, e.detail.group, e.detail.option);
    this.limit_ = PAGE_SIZE;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.query_ = e.detail;
    this.limit_ = PAGE_SIZE;
  }

  // ZenLibraryDownloadsSection #computeVisible: newest first, of the types
  // and the time chosen, matching the search, a page at a time.
  protected visible_(): Download[] {
    const types =
        FILE_GROUPS.filter(id => this.active_.includes(filterKey('type', id)));
    const when = Object.keys(WHEN_DAYS).find(
        id => this.active_.includes(filterKey('when', id)));
    const cutoff = when ? Date.now() - WHEN_DAYS[when]! * MS_PER_DAY : 0;
    const query = this.query_.toLowerCase();
    const visible: Download[] = [];
    for (let i = this.downloads_.length - 1; i >= 0; i--) {
      const download = this.downloads_[i]!;
      if (types.length && !types.includes(fileGroupOf(download.fileName) ?? '')) {
        continue;
      }
      if (cutoff && !(download.endTime >= cutoff)) {
        continue;
      }
      if (query && !download.fileName.toLowerCase().includes(query) &&
          !download.url.toLowerCase().includes(query)) {
        continue;
      }
      visible.push(download);
      if (visible.length >= this.limit_) {
        break;
      }
    }
    return visible;
  }

  // Template helpers.

  protected isEmpty_(): boolean {
    return this.loaded_ && !this.visible_().length;
  }

  protected isPending_(download: Download): boolean {
    return isPending(download);
  }

  protected hasProgress_(download: Download): boolean {
    return hasProgress(download);
  }

  protected hasFile_(download: Download): boolean {
    return hasFile(download);
  }

  protected canRetry_(download: Download): boolean {
    return canRetry(download);
  }


  protected iconFor_(download: Download): string {
    if (download.previewUrl) {
      return `url("${download.previewUrl}"), url("${this.fileIcon_(download)}")`;
    }
    return `url("${this.fileIcon_(download)}")`;
  }

  private fileIcon_(download: Download): string {
    return `chrome://fileicon/?path=${
        encodeURIComponent(download.path || download.fileName)}&scale=2x`;
  }

  // The arc round a download's icon: dashes are its percentage of the way
  // there; one with no total keeps a quarter and spins.
  protected ringDash_(download: Download): string {
    return `${hasProgress(download) ? Math.round(progressOf(download)) : 25} 100`;
  }

  protected statusLabel_(download: Download): string {
    return isPending(download) && download.openWhenComplete ?
        this.strings.openWhenDone :
        statusText(download, Date.now());
  }

  protected urlLabel_(download: Download): string {
    return formatUrl(download.url);
  }

  // Rows.

  private downloadOf_(e: Event): Download|undefined {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    return this.downloads_.find(download => download.id === id);
  }

  private do_(download: Download, command: DownloadCommand) {
    this.proxy_.handler.doDownload(download.id, command);
  }

  // A click on one still coming in asks for it to be opened when done; a
  // held back one asks first.
  protected async onRowClick_(e: Event) {
    const download = this.downloadOf_(e);
    if (!download) {
      return;
    }
    if (download.dangerous) {
      await this.unblock_(download);
      return;
    }
    this.do_(download, DownloadCommand.kOpen);
  }

  private async unblock_(download: Download) {
    const keep = await this.library?.confirm(
        this.strings.unblockTitle, this.strings.unblockMessage,
        this.strings.unblockAccept);
    if (keep) {
      this.do_(download, DownloadCommand.kKeepDangerous);
    }
  }

  protected onCancelClick_(e: Event) {
    e.stopPropagation();
    const download = this.downloadOf_(e);
    if (download) {
      this.do_(download, DownloadCommand.kCancel);
    }
  }

  protected onRetryClick_(e: Event) {
    e.stopPropagation();
    const download = this.downloadOf_(e);
    if (download) {
      this.do_(download, DownloadCommand.kRetry);
    }
  }

  protected onShowClick_(e: Event) {
    e.stopPropagation();
    const download = this.downloadOf_(e);
    if (download) {
      this.do_(download, DownloadCommand.kShow);
    }
  }

  protected onMoreClick_(e: MouseEvent) {
    e.stopPropagation();
    const download = this.downloadOf_(e);
    if (download) {
      const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
      this.openMenu_(download, rect.left, rect.bottom + 4);
    }
  }

  protected onRowContextmenu_(e: MouseEvent) {
    e.preventDefault();
    const download = this.downloadOf_(e);
    if (download) {
      this.openMenu_(download, e.clientX, e.clientY);
    }
  }

  // ZenLibraryDownloadsSection's menu, with what cannot be done left out.
  private async openMenu_(download: Download, x: number, y: number) {
    const s = this.strings;
    const file = hasFile(download);
    const items: MenuItem[] = [
      ...(file ? [
        {
          label: s.menuOpen(download.fileName),
          action: () => this.do_(download, DownloadCommand.kOpen),
        },
        {
          label: s.showInFinder,
          action: () => this.do_(download, DownloadCommand.kShow),
        },
        {label: ''},
        {
          label: s.menuCopy(download.fileName),
          action: () => this.do_(download, DownloadCommand.kCopyFile),
        },
      ] :
                 []),
      {
        label: s.menuCopyLink,
        action: () => this.do_(download, DownloadCommand.kCopyLink),
      },
      {label: ''},
      ...(download.dangerous ?
              [{label: s.menuUnblock, action: () => this.unblock_(download)}] :
              []),
      ...(!isPending(download) ?
              [{
                label: s.menuHide,
                action: () => this.do_(download, DownloadCommand.kHide),
              }] :
              []),
      ...(file || isPending(download) ?
              [{
                label: s.menuDelete,
                action: () => this.do_(download, DownloadCommand.kDeleteFile),
              }] :
              []),
    ];
    this.menuRow_ = download.id;
    await showMenu(x, y, items);
    this.menuRow_ = -1;
  }

  // The file drags as the system drags files, onto the page or another app.
  protected onRowDragstart_(e: DragEvent) {
    e.preventDefault();
    const download = this.downloadOf_(e);
    if (download && hasFile(download)) {
      this.do_(download, DownloadCommand.kDrag);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-downloads': LibraryDownloadsElement;
  }
}

customElements.define(LibraryDownloadsElement.is, LibraryDownloadsElement);
