// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {HistorySort} from '../sidebar.mojom-webui.js';
import type {ArchivedTab, HistoryVisit} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import type {LibraryElement} from './library.js';
import type {FilterToggle} from './library_filters.js';
import {getHtml} from './library_history.html.js';
import {arrangeClosed, formatDay, formatUrl, formatVisitTime, groupByDay, groupBySite, HISTORY_DAYS_OLD, PAGE_SIZE, siteOf, WHEN_DAYS} from './library_model.js';
import type {Visit, VisitGroup} from './library_model.js';
import {filterKey, toggleFilter, updateFilterHeight, watchResults} from './library_search.js';
import type {FilterGroup} from './library_search.js';
import {libraryStrings} from './library_strings.js';
import {showMenu} from './menu.js';
import './library_filters.js';
import './library_search_box.js';

const SORTS: Record<string, HistorySort> = {
  date: HistorySort.kDate,
  site: HistorySort.kSite,
  mostvisited: HistorySort.kMostVisited,
  lastvisited: HistorySort.kLastVisited,
};
// The archive keeps 90 days; the whole of it is read at once.
const ARCHIVE_LIMIT = 5000;

// zen:library/sections/ZenLibraryHistorySection.mjs: history by day (or
// site, or visits), or, as its "Closed tabs" source, the space's archive of
// tabs closed by Cromarc and by hand. Draws into the Library's shadow root.
export class LibraryHistoryElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-history';
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
      groups_: {type: Array},
      loaded_: {type: Boolean},
      filtersOpen_: {type: Boolean},
      active_: {type: Array},
      menuRow_: {type: String},
      strings: {type: Object},
    };
  }

  accessor library: LibraryElement|null = null;
  protected accessor groups_: VisitGroup[] = [];
  protected accessor loaded_: boolean = false;
  protected accessor filtersOpen_: boolean = false;
  protected accessor active_: string[] = [];
  // The row whose menu is open keeps its hover look.
  protected accessor menuRow_: string = '';
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private query_ = '';
  private limit_ = PAGE_SIZE;
  private exhausted_ = false;
  private generation_ = 0;
  private stopWatching_ = () => {};
  private listener_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.listener_ = this.proxy_.callbackRouter.onHistoryChanged.addListener(
        () => this.fetch_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopWatching_();
    if (this.listener_ !== null) {
      this.proxy_.callbackRouter.removeListener(this.listener_);
      this.listener_ = null;
    }
  }

  override firstUpdated(changed: PropertyValues<this>) {
    super.firstUpdated(changed);
    this.active_ = [
      'sort:date',
      this.library?.closedTabs() ? 'source:closed' : 'source:history',
    ];
    this.stopWatching_ = watchResults(this, () => this.onListScrolledToEnd_());
    this.fetch_();
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('filtersOpen_' as keyof LibraryHistoryElement) &&
        this.filtersOpen_) {
      updateFilterHeight(this);
    }
  }

  // From the Library.

  onShown() {
    this.focusSearch_();
  }

  onLibraryOpened() {
    this.focusSearch_();
  }

  onEscape(): boolean {
    if (!this.filtersOpen_) {
      return false;
    }
    this.filtersOpen_ = false;
    return true;
  }

  private focusSearch_() {
    this.updateComplete.then(
        () => this.querySelector<HTMLElement>('.search-box')?.focus());
  }

  // Filters.

  private isActive_(group: string, option: string): boolean {
    return this.active_.includes(filterKey(group, option));
  }

  protected showingClosed_(): boolean {
    return this.isActive_('source', 'closed');
  }

  protected filterGroups_(): FilterGroup[] {
    const s = this.strings;
    return [
      {
        id: 'when',
        title: s.historyFilterWhen,
        exclusive: true,
        options: [
          {id: 'today', label: s.today},
          {id: 'week', label: s.week},
          {id: 'month', label: s.month},
        ],
      },
      {
        id: 'sort',
        title: s.historyFilterSort,
        exclusive: true,
        options: [
          {id: 'date', label: s.sortDate},
          {id: 'site', label: s.sortSite},
          // A tab that was closed was never counted as visited.
          {
            id: 'mostvisited',
            label: s.sortMostVisited,
            disabled: this.showingClosed_(),
          },
          {id: 'lastvisited', label: s.sortLastVisited},
        ],
      },
      {
        id: 'source',
        title: s.historyFilterSource,
        exclusive: true,
        options: [
          {id: 'history', label: s.sourceHistory},
          {id: 'closed', label: s.sourceClosed},
        ],
      },
    ];
  }

  private sort_(): string {
    return Object.keys(SORTS).find(id => this.isActive_('sort', id)) ?? 'date';
  }

  private whenDays_(): number|null {
    const id = Object.keys(WHEN_DAYS).find(id => this.isActive_('when', id));
    return id ? WHEN_DAYS[id]! : null;
  }

  protected onFilterButtonClick_() {
    this.filtersOpen_ = true;
  }

  protected onFilterDoneClick_() {
    this.filtersOpen_ = false;
  }

  protected onFilterToggled_(e: CustomEvent<FilterToggle>) {
    const wasClosed = this.showingClosed_();
    let active = toggleFilter(this.active_, e.detail.group, e.detail.option);
    if (!active.some(key => key.startsWith('source:'))) {
      active = [...active, 'source:history'];
    }
    const closed = active.includes('source:closed');
    if (closed && active.includes('sort:mostvisited')) {
      active = active.filter(key => key !== 'sort:mostvisited');
    }
    if (!active.some(key => key.startsWith('sort:'))) {
      active = [...active, 'sort:date'];
    }
    this.active_ = active;
    if (closed !== wasClosed) {
      this.fire('closed-tabs-changed', closed);
    }
    this.resetAndFetch_();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.query_ = e.detail;
    this.resetAndFetch_();
  }

  // Reading.

  private onListScrolledToEnd_() {
    if (this.exhausted_ || !this.loaded_) {
      return;
    }
    this.limit_ += PAGE_SIZE;
    this.fetch_();
  }

  private resetAndFetch_() {
    this.limit_ = PAGE_SIZE;
    this.exhausted_ = false;
    this.fetch_();
  }

  private async fetch_() {
    const generation = ++this.generation_;
    const sort = this.sort_();
    if (this.showingClosed_()) {
      const {tabs} =
          await this.proxy_.handler.searchArchive(this.query_, ARCHIVE_LIMIT);
      if (generation !== this.generation_) {
        return;
      }
      this.exhausted_ = true;
      this.groups_ = arrangeClosed(
          tabs.map((tab: ArchivedTab) => ({
                     url: tab.url,
                     title: tab.title || tab.url,
                     time: tab.closedAt,
                     archiveId: tab.id,
                   })),
          sort, this.whenDays_() ?? HISTORY_DAYS_OLD, '', Date.now());
      this.loaded_ = true;
      return;
    }
    // A search looks through all of history unless a time is chosen.
    const days =
        this.whenDays_() ?? (this.query_ ? 0 : HISTORY_DAYS_OLD);
    const {visits} = await this.proxy_.handler.queryHistory(
        this.query_, days, this.limit_, SORTS[sort]!);
    if (generation !== this.generation_) {
      return;
    }
    this.exhausted_ = visits.length < this.limit_;
    const rows = visits.map((visit: HistoryVisit) => ({
                              url: visit.url,
                              title: visit.title,
                              time: visit.time,
                            }));
    if (this.query_ || sort === 'mostvisited' || sort === 'lastvisited') {
      this.groups_ = [{key: 'all', visits: rows}];
    } else {
      this.groups_ = sort === 'site' ? groupBySite(rows) : groupByDay(rows);
    }
    this.loaded_ = true;
  }

  // Template helpers.

  protected isEmpty_(): boolean {
    return this.loaded_ && this.groups_.every(group => !group.visits.length);
  }

  protected groupTitle_(group: VisitGroup): string {
    if (group.day !== undefined) {
      return formatDay(group.day, Date.now());
    }
    if (group.site !== undefined) {
      return group.site || this.strings.siteOther;
    }
    return '';
  }

  protected visitKey_(visit: Visit): string {
    return visit.archiveId !== undefined ? `archive-${visit.archiveId}` :
                                           `${visit.url}-${visit.time}`;
  }

  protected faviconFor_(visit: Visit): string {
    return getFaviconForPageURL(visit.url, false);
  }

  protected urlLabel_(visit: Visit): string {
    return formatUrl(visit.url);
  }

  protected timeLabel_(visit: Visit): string {
    return formatVisitTime(visit.time);
  }

  // Rows.

  private visitOf_(e: Event): Visit|null {
    const key = (e.currentTarget as HTMLElement).dataset['key'];
    for (const group of this.groups_) {
      const visit = group.visits.find(visit => this.visitKey_(visit) === key);
      if (visit) {
        return visit;
      }
    }
    return null;
  }

  // Holding Command, or the middle button, leaves the tab in the background
  // and the Library open.
  private openVisit_(visit: Visit, background: boolean) {
    if (visit.archiveId === undefined) {
      this.library?.openPage(visit.url, background);
      return;
    }
    if (background) {
      this.fire('library-keep-open');
      this.library?.showToast(this.strings.openedInBackground);
    }
    this.proxy_.handler.restoreFromArchive(
        visit.archiveId, visit.url, background);
    if (!background) {
      this.fire('library-close');
    }
  }

  protected onRowClick_(e: MouseEvent) {
    const visit = this.visitOf_(e);
    if (visit) {
      this.openVisit_(visit, e.metaKey);
    }
  }

  protected onRowAuxclick_(e: MouseEvent) {
    const visit = this.visitOf_(e);
    if (visit && e.button === 1) {
      e.preventDefault();
      this.openVisit_(visit, true);
    }
  }

  protected onForgetClick_(e: MouseEvent) {
    e.stopPropagation();
    const visit = this.visitOf_(e);
    if (visit) {
      this.forget_(visit);
    }
  }

  protected onReopenClick_(e: MouseEvent) {
    e.stopPropagation();
    const visit = this.visitOf_(e);
    if (visit) {
      this.openVisit_(visit, e.metaKey);
    }
  }

  private forget_(visit: Visit) {
    if (visit.archiveId !== undefined) {
      this.proxy_.handler.removeFromArchive(visit.archiveId);
    } else {
      this.proxy_.handler.forgetPage(visit.url);
    }
    const gone = this.visitKey_(visit);
    this.groups_ =
        this.groups_
            .map(group => ({
                   ...group,
                   visits: group.visits.filter(
                       other => visit.archiveId === undefined ?
                           other.url !== visit.url :
                           this.visitKey_(other) !== gone),
                 }))
            .filter(group => group.visits.length);
  }

  protected async onRowContextmenu_(e: MouseEvent) {
    e.preventDefault();
    const visit = this.visitOf_(e);
    if (!visit) {
      return;
    }
    const site = siteOf(visit.url);
    this.menuRow_ = this.visitKey_(visit);
    await showMenu(e.clientX, e.clientY, [
      {label: this.strings.menuReopen, action: () => this.openVisit_(visit, false)},
      {label: ''},
      {label: this.strings.menuForget, action: () => this.forget_(visit)},
      ...(!this.showingClosed_() && site ?
              [{
                label: this.strings.menuForgetSite,
                action: () => this.forgetSite_(visit, site),
              }] :
              []),
    ]);
    this.menuRow_ = '';
  }

  private async forgetSite_(visit: Visit, site: string) {
    const accepted = await this.library?.confirm(
        this.strings.forgetSiteTitle, this.strings.forgetSiteMessage(site),
        this.strings.forgetSiteAccept);
    if (accepted) {
      this.proxy_.handler.forgetSite(visit.url);
    }
  }

  // A visit drags as a link, onto the page, the sidebar or another app.
  protected onRowDragstart_(e: DragEvent) {
    const visit = this.visitOf_(e);
    if (!visit || !e.dataTransfer) {
      return;
    }
    e.dataTransfer.setData('text/uri-list', visit.url);
    e.dataTransfer.setData('text/plain', visit.url);
    e.dataTransfer.effectAllowed = 'copyLink';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-history': LibraryHistoryElement;
  }
}

customElements.define(LibraryHistoryElement.is, LibraryHistoryElement);
