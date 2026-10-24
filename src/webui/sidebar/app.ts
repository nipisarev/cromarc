// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabLifecycle, TreeNode} from '../sidebar.mojom-webui.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {buildSidebar, dropTarget} from './sidebar_model.js';
import type {DropPosition, PinnedRow, SidebarView, TreeRow} from './sidebar_model.js';
import {strings} from './strings.js';
import {TabListModel} from './tab_list_model.js';

export interface MenuItem {
  label: string;
  action: () => void;
}

export interface Menu {
  x: number;
  y: number;
  items: MenuItem[];
}

// What is being dragged: a tree item (pinned or folder) or a Today tab.
interface DragSource {
  kind: 'item'|'tab';
  id: string;
}

export interface DropIndicator {
  id: string;
  position: DropPosition;
}

export class SidebarAppElement extends CrLitElement {
  static get is() {
    return 'cromarc-sidebar-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabs_: {type: Array},
      tree_: {type: Array},
      view_: {type: Object},
      menu_: {type: Object},
      renaming_: {type: String},
      drop_: {type: Object},
      strings: {type: Object},
      minutesLeft_: {type: Object},
      cleared_: {type: Array},
    };
  }

  protected accessor tabs_: Tab[] = [];
  protected accessor tree_: TreeNode[] = [];
  protected accessor view_: SidebarView =
      {essentials: [], rows: [], attention: [], today: []};
  protected accessor menu_: Menu|null = null;
  // Id of the item whose title is being edited.
  protected accessor renaming_: string = '';
  protected accessor drop_: DropIndicator|null = null;
  // Today tabs close to the archive: tab id → minutes left.
  protected accessor minutesLeft_: Map<string, number> = new Map();
  // URLs closed by the last Clear while its undo toast shows.
  protected accessor cleared_: string[] = [];
  private deferred_: Set<string> = new Set();
  private clearedTimer_ = 0;

  protected accessor strings: typeof strings = strings;
  private readonly proxy_ = BrowserProxy.getInstance();
  // Bumped on every TabStripService snapshot; older observers go quiet.
  private generation_ = 0;
  private drag_: DragSource|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.callbackRouter.onTreeChanged.addListener(
        (nodes: TreeNode[]) => this.tree_ = nodes);
    this.proxy_.handler.getTree().then(({nodes}) => this.tree_ = nodes);
    this.proxy_.callbackRouter.onTabLifecyclesChanged.addListener(
        (tabs: TabLifecycle[]) => this.setLifecycles_(tabs));
    this.proxy_.handler.getTabLifecycles().then(
        ({tabs}) => this.setLifecycles_(tabs));
    this.connectTabs_();
    document.addEventListener('keydown', e => {
      if (e.key === 'Escape') {
        this.menu_ = null;
      }
    });
  }

  override willUpdate(changed: PropertyValues<this>) {
    super.willUpdate(changed);
    if (changed.has('tabs_' as keyof SidebarAppElement) ||
        changed.has('tree_' as keyof SidebarAppElement) ||
        changed.has('minutesLeft_' as keyof SidebarAppElement)) {
      this.view_ = buildSidebar(this.tree_, this.tabs_, this.deferred_);
    }
  }

  private setLifecycles_(tabs: TabLifecycle[]) {
    this.deferred_ = new Set(tabs.filter(t => t.deferred).map(t => t.tab));
    // A new Map also re-runs buildSidebar() for the deferred set.
    this.minutesLeft_ = new Map(tabs.filter(t => t.minutesLeft > 0)
                                    .map(t => [t.tab, t.minutesLeft]));
  }

  // Reads the window's tab strip and follows its events; re-reads it when an
  // event does not fit, so the sidebar never drifts from the window.
  private async connectTabs_() {
    const generation = ++this.generation_;
    const snapshot = await this.proxy_.tabStrip.getTabs();
    if (generation !== this.generation_) {
      return;
    }
    const model = new TabListModel(snapshot.tabStrip);
    const apply = (ok: boolean) => {
      if (generation !== this.generation_) {
        return;
      }
      if (!ok) {
        this.connectTabs_();
        return;
      }
      this.tabs_ = model.tabs();
    };
    const observer: TabStripObserver = {
      onTabsCreated: event => apply(model.onTabsCreated(event)),
      onNodesClosed: event => apply(model.onNodesClosed(event)),
      onNodeMoved: event => apply(model.onNodeMoved(event)),
      onCollectionCreated: event => apply(model.onCollectionCreated(event)),
      onDataChanged: event => {
        if (whichOnDataChangedEvent(event) === OnDataChangedEventFieldTags.TAB) {
          apply(model.onTabChanged(event.tab!));
        }
      },
    };
    this.tabs_ = model.tabs();
    new TabStripObservation(observer).bind(snapshot.stream.handle);
  }

  // Template helpers.

  protected faviconFor_(url: string, tab: Tab|null): string {
    if (tab && tab.favicon.dataUrl) {
      return `url("${tab.favicon.dataUrl}")`;
    }
    return getFaviconForPageURL(url, false);
  }

  protected isActive_(tab: Tab|null): boolean {
    return !!tab && tab.isActive;
  }

  protected dropClass_(id: string): string {
    return this.drop_?.id === id ? `drop-${this.drop_.position}` : '';
  }

  protected pinnedTooltip_(row: PinnedRow): string {
    return row.state === 'away' && row.tab ? row.tab.url : row.homeUrl;
  }

  protected todayTooltip_(tab: Tab): string {
    const minutes = this.minutesLeft_.get(tab.id);
    return minutes ? `${tab.title}\n${strings.archivesIn(minutes)}` : tab.title;
  }

  // Width of the countdown bar: grows over the last hour.
  protected countdown_(tab: Tab): string {
    const minutes = this.minutesLeft_.get(tab.id);
    return minutes ? `${Math.round((60 - minutes) / 60 * 100)}%` : '0';
  }

  // Clicks.

  protected onItemClick_(e: Event) {
    const id = this.idOf_(e);
    if (id && id !== this.renaming_) {
      this.proxy_.handler.openItem(id);
    }
  }

  protected onFolderClick_(e: Event) {
    const row = this.rowOf_(e);
    if (row && row.kind === 'folder' && row.id !== this.renaming_) {
      this.proxy_.handler.setCollapsed(row.id, !row.collapsed);
    }
  }

  protected onTodayClick_(e: Event) {
    const id = this.idOf_(e);
    if (id) {
      // The tab may close before the call lands; the next event resyncs.
      this.proxy_.tabStrip.activateTab(id).catch(() => {});
    }
  }

  protected onCloseTabClick_(e: Event) {
    e.stopPropagation();
    const tabId = (e.currentTarget as HTMLElement).dataset['tab'];
    if (tabId) {
      this.proxy_.tabStrip.closeNodes([tabId]).catch(() => {});
    }
  }

  protected onResetClick_(e: Event) {
    e.stopPropagation();
    const id = this.idOf_(e);
    if (id) {
      this.proxy_.handler.resetItem(id);
    }
  }

  protected onNewTabClick_() {
    this.proxy_.handler.newTab();
  }

  // Today's Clear: everything goes to the archive; a toast offers undo for
  // 5 seconds (design, UI section 4).
  protected async onClearClick_() {
    const {closed} = await this.proxy_.handler.clearToday();
    if (closed.length === 0) {
      return;
    }
    this.cleared_ = closed;
    clearTimeout(this.clearedTimer_);
    this.clearedTimer_ = setTimeout(() => this.cleared_ = [], 5000);
  }

  protected onUndoClearClick_() {
    this.proxy_.handler.reopenTabs(this.cleared_);
    this.cleared_ = [];
    clearTimeout(this.clearedTimer_);
  }

  protected onKeepClick_(e: Event) {
    e.stopPropagation();
    const id = this.idOf_(e);
    if (id) {
      this.proxy_.handler.keepTab(id);
    }
  }

  // Context menus.

  protected onItemContextmenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const row = this.rowOf_(e) ?? this.essentialOf_(e);
    if (!row) {
      return;
    }
    const h = this.proxy_.handler;
    const items: MenuItem[] = [];
    if (row.kind === 'folder') {
      items.push(
          {label: strings.newFolder, action: () => this.createFolder_(row.id)},
          {label: strings.rename, action: () => this.renaming_ = row.id},
          {label: strings.remove, action: () => h.removeItem(row.id)});
    } else {
      if (row.state === 'away') {
        items.push({label: strings.reset, action: () => h.resetItem(row.id)},
                   {label: strings.makeHome, action: () => h.setHomeToCurrent(row.id)});
      }
      items.push(
          {
            label: row.essential ? strings.removeFromEssentials :
                                   strings.addToEssentials,
            action: () => h.setEssential(row.id, !row.essential),
          },
          {label: strings.rename, action: () => this.renaming_ = row.id},
          {label: strings.copyLink, action: () => this.copy_(row.homeUrl)},
          {label: strings.unpin, action: () => h.removeItem(row.id)});
    }
    this.menu_ = {x: e.clientX, y: e.clientY, items};
  }

  protected onTodayContextmenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const tab = this.todayOf_(e);
    if (!tab) {
      return;
    }
    const h = this.proxy_.handler;
    this.menu_ = {
      x: e.clientX,
      y: e.clientY,
      items: [
        {label: strings.pin, action: () => h.pinTab(tab.id, '', this.rootSize_(), false)},
        {label: strings.addToEssentials, action: () => h.pinTab(tab.id, '', this.rootSize_(), true)},
        {label: strings.copyLink, action: () => this.copy_(tab.url)},
        {label: strings.close, action: () => this.proxy_.tabStrip.closeNodes([tab.id]).catch(() => {})},
      ],
    };
  }

  protected onBackgroundContextmenu_(e: MouseEvent) {
    e.preventDefault();
    this.menu_ = {
      x: e.clientX,
      y: e.clientY,
      items: [{label: strings.newFolder, action: () => this.createFolder_('')}],
    };
  }

  protected onMenuItemClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const item = this.menu_?.items[index];
    this.menu_ = null;
    item?.action();
  }

  protected onMenuBackdropClick_() {
    this.menu_ = null;
  }

  protected onMenuBackdropContextmenu_(e: Event) {
    e.preventDefault();
    this.menu_ = null;
  }

  private async createFolder_(parentId: string) {
    const index = parentId ? 0 : this.rootSize_();
    const {id} = await this.proxy_.handler.createFolder(parentId, index, strings.newFolder);
    if (id) {
      this.renaming_ = id;
    }
  }

  private copy_(text: string) {
    navigator.clipboard.writeText(text).catch(() => {});
  }

  // Renaming.

  protected onTitleDblclick_(e: Event) {
    e.stopPropagation();
    const id = this.idOf_(e);
    if (id) {
      this.renaming_ = id;
    }
  }

  protected onRenameKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      (e.target as HTMLInputElement).blur();
    } else if (e.key === 'Escape') {
      this.renaming_ = '';
    }
  }

  protected onRenameClick_(e: Event) {
    e.stopPropagation();
  }

  protected onRenameBlur_(e: Event) {
    const input = e.target as HTMLInputElement;
    const id = this.renaming_;
    this.renaming_ = '';
    const title = input.value.trim();
    if (id && title) {
      this.proxy_.handler.renameItem(id, title);
    }
  }

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('renaming_' as keyof SidebarAppElement) && this.renaming_) {
      const input = this.shadowRoot.querySelector<HTMLInputElement>('input.rename');
      input?.focus();
      input?.select();
    }
  }

  // Drag and drop (Arc): a Today tab dropped into the tree is pinned there, a
  // tree item moves, anything dropped on the Essentials grid becomes an
  // essential, a pinned item dropped on Today is unpinned.

  protected onDragstart_(e: DragEvent) {
    const el = e.currentTarget as HTMLElement;
    const kind = el.dataset['kind'] === 'tab' ? 'tab' : 'item';
    const id = el.dataset['id'];
    if (!id) {
      return;
    }
    this.drag_ = {kind, id};
    e.dataTransfer!.effectAllowed = 'move';
    e.dataTransfer!.setData('text/plain', id);
  }

  protected onDragend_() {
    this.drag_ = null;
    this.drop_ = null;
  }

  protected onRowDragover_(e: DragEvent) {
    const row = this.rowOf_(e);
    if (!this.drag_ || !row || row.id === this.drag_.id) {
      return;
    }
    e.preventDefault();
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const y = (e.clientY - rect.top) / rect.height;
    let position: DropPosition = y < 0.5 ? 'before' : 'after';
    if (row.kind === 'folder' && y > 0.25 && y < 0.75) {
      position = 'inside';
    }
    this.drop_ = {id: row.id, position};
  }

  protected onRowDrop_(e: DragEvent) {
    e.preventDefault();
    const row = this.rowOf_(e);
    const drag = this.drag_;
    const drop = this.drop_;
    this.onDragend_();
    if (!row || !drag || !drop) {
      return;
    }
    const {parentId, index} = dropTarget(row, drop.position);
    if (drag.kind === 'tab') {
      this.proxy_.handler.pinTab(drag.id, parentId, index, false);
    } else {
      this.proxy_.handler.moveItem(drag.id, parentId, index);
    }
  }

  protected onEssentialsDragover_(e: DragEvent) {
    if (this.drag_) {
      e.preventDefault();
    }
  }

  protected onEssentialsDrop_(e: DragEvent) {
    e.preventDefault();
    const drag = this.drag_;
    this.onDragend_();
    if (!drag) {
      return;
    }
    if (drag.kind === 'tab') {
      this.proxy_.handler.pinTab(drag.id, '', this.rootSize_(), true);
    } else {
      this.proxy_.handler.setEssential(drag.id, true);
    }
  }

  protected onTreeEndDragover_(e: DragEvent) {
    if (this.drag_) {
      e.preventDefault();
    }
  }

  protected onTreeEndDrop_(e: DragEvent) {
    e.preventDefault();
    const drag = this.drag_;
    this.onDragend_();
    if (!drag) {
      return;
    }
    if (drag.kind === 'tab') {
      this.proxy_.handler.pinTab(drag.id, '', this.rootSize_(), false);
    } else {
      this.proxy_.handler.setEssential(drag.id, false);
      this.proxy_.handler.moveItem(drag.id, '', this.rootSize_());
    }
  }

  protected onTodayDragover_(e: DragEvent) {
    if (this.drag_?.kind === 'item') {
      e.preventDefault();
    }
  }

  protected onTodayDrop_(e: DragEvent) {
    e.preventDefault();
    const drag = this.drag_;
    this.onDragend_();
    if (drag?.kind === 'item') {
      this.proxy_.handler.removeItem(drag.id);
    }
  }

  // Lookups.

  private idOf_(e: Event): string|undefined {
    return (e.currentTarget as HTMLElement).closest<HTMLElement>('[data-id]')
        ?.dataset['id'];
  }

  private rowOf_(e: Event): TreeRow|undefined {
    const id = this.idOf_(e);
    return this.view_.rows.find(r => r.id === id);
  }

  private essentialOf_(e: Event): PinnedRow|undefined {
    const id = this.idOf_(e);
    return this.view_.essentials.find(r => r.id === id);
  }

  private todayOf_(e: Event): Tab|undefined {
    const id = this.idOf_(e);
    return this.view_.today.find(t => t.id === id);
  }

  private rootSize_(): number {
    return this.tree_.length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-sidebar-app': SidebarAppElement;
  }
}

customElements.define(SidebarAppElement.is, SidebarAppElement);
