// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {SwipeType} from '../sidebar.mojom-webui.js';
import type {ICloudSpace, Space, SpaceSnapshot, Swipe, TabLifecycle, TreeNode} from '../sidebar.mojom-webui.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import './library.js';
import './space_editor.js';
import './theme_editor.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {buildSidebar, dropTarget} from './sidebar_model.js';
import type {SpaceEdit} from './space_editor.js';
import {iconMask} from './space_icon.js';
import {gradientOf, themeFromJson} from './theme_model.js';
import type {SpaceTheme} from './theme_model.js';
import {ADD_SWIPE_ENTRY, ADD_SWIPE_SHIFT, ADD_SWIPE_TRAVEL, addSwipeProgress, circularDiff, easeInOut, isAnimatedStrip, moveSpace, neighbourIndex, spring, SWITCH_DURATION, SwipeState} from './spaces.js';
import type {DropPosition, PinnedRow, SidebarView, TreeRow} from './sidebar_model.js';
import {strings} from './strings.js';
import {TabListModel} from './tab_list_model.js';

// The space form open over the strip: a new space, or `id`'s settings.
export interface SpaceEditorState {
  id: string;
  name: string;
  icon: string;
  pickerOpen: boolean;
}

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

// Another space's strip content, built from its snapshot.
export interface SnapshotView {
  view: SidebarView;
  today: Array<{title: string, url: string, favicon: string}>;
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
      dropZone_: {type: String},
      resetAccel_: {type: Boolean},
      strings: {type: Object},
      minutesLeft_: {type: Object},
      cleared_: {type: Array},
      spaces_: {type: Array},
      snapshots_: {type: Object},
      stripX_: {type: Array},
      tint_: {type: String},
      editor_: {type: Object},
      deleting_: {type: Object},
      icloud_: {type: Object},
      themeEditor_: {type: Object},
      themePreview_: {type: Object},
      remoteSpaces_: {type: Array},
      libraryShown_: {type: Boolean},
      libraryOpen_: {type: Boolean},
      libraryDrag_: {type: Number},
      addProgress_: {type: Number},
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
  // The space header or the end of the tree under a drag.
  protected accessor dropZone_: ''|'header'|'end' = '';
  // Cmd is held over a reset button: its sublabel offers to separate.
  protected accessor resetAccel_: boolean = false;
  // Today tabs close to the archive: tab id → minutes left.
  protected accessor minutesLeft_: Map<string, number> = new Map();
  // URLs closed by the last Clear while its undo toast shows.
  protected accessor cleared_: string[] = [];
  // Spaces in order; each has a strip (design, component 4).
  protected accessor spaces_: Space[] = [];
  protected accessor snapshots_: Map<string, SnapshotView> = new Map();
  // Horizontal position of every strip, in % of the sidebar width.
  protected accessor stripX_: number[] = [];
  // Background tint: the space's colour, blended while switching.
  protected accessor tint_: string = 'transparent';
  protected accessor editor_: SpaceEditorState|null = null;
  // The space whose deletion waits for confirmation.
  protected accessor deleting_: Space|null = null;
  private draggedSpace_ = '';
  // The iCloud Drive card of a space: on from the next start (`enabled`),
  // and now (`active`).
  protected accessor icloud_:
      {space: Space, enabled: boolean, active: boolean}|null = null;
  // Spaces in iCloud Drive this Mac does not have, offered in the new space
  // form.
  protected accessor remoteSpaces_: ICloudSpace[] = [];
  // The theme editor open over the strip, and its unsaved theme.
  protected accessor themeEditor_: {space: Space, theme: SpaceTheme}|null =
      null;
  protected accessor themePreview_: {id: string, theme: SpaceTheme}|null =
      null;
  // The Library: rendered while open, dragged in or closing.
  protected accessor libraryShown_: boolean = false;
  protected accessor libraryOpen_: boolean = false;
  // How far a swipe has pulled the Library in, 0–1; -1 when no swipe does.
  protected accessor libraryDrag_: number = -1;
  // Zen's add-space swipe past the last space: 1 is ready.
  protected accessor addProgress_: number = 0;
  // What a swipe at the edge of the spaces does (zen ZenSpacesSwipe actions).
  private edgeAction_: 'library'|'add'|null = null;
  private edgeAllowed_ = {library: false, add: false};
  private libraryTimer_ = 0;
  private deferred_: Set<string> = new Set();
  private clearedTimer_ = 0;
  private swipe_: SwipeState|null = null;
  private swipeOffset_ = 0;
  private animationFrame_ = 0;
  private tintRgb_: number[] = [0, 0, 0];

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
    const router = this.proxy_.callbackRouter;
    router.onSpacesChanged.addListener(
        (spaces: Space[]) => this.setSpaces_(spaces));
    router.onSwitchRequested.addListener((id: string) => this.switchTo_(id));
    router.onSwipe.addListener((swipe: Swipe) => this.onSwipe_(swipe));
    this.proxy_.handler.getSpaces().then(({spaces}) => this.setSpaces_(spaces));
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

  // Spaces.

  protected get currentIndex_(): number {
    return Math.max(0, this.spaces_.findIndex(s => s.current));
  }

  private setSpaces_(spaces: Space[]) {
    if (this.themePreview_ && !this.themeEditor_) {
      this.themePreview_ = null;
    }
    this.spaces_ = spaces;
    this.snapshots_ = new Map();
    this.rest_();
  }

  // Every strip in its place for this window's own space.
  private rest_() {
    cancelAnimationFrame(this.animationFrame_);
    const current = this.currentIndex_;
    const count = this.spaces_.length;
    this.stripX_ = this.spaces_.map(
        (_, i) => circularDiff(i, current, count) * 100);
    this.setTint_(this.colorOf_(current), this.colorOf_(current), 0);
  }

  private colorOf_(index: number): number[] {
    const argb = this.spaces_[index]?.color ?? 0;
    return [(argb >>> 16) & 255, (argb >>> 8) & 255, argb & 255];
  }

  // Zen fades the background between the two spaces' themes as the strips
  // move (--zen-background-opacity = 1 - |offset| / 200).
  private setTint_(from: number[], to: number[], progress: number) {
    this.tintRgb_ = from.map((v, i) => v + (to[i]! - v) * progress);
    this.tint_ = `rgba(${this.tintRgb_.map(Math.round).join(', ')}, 0.16)`;
  }

  // The other spaces' content, for their strips while switching.
  private async loadSnapshots_() {
    const snapshots = new Map(this.snapshots_);
    for (const space of this.spaces_) {
      if (space.current || snapshots.has(space.id)) {
        continue;
      }
      const {snapshot} = await this.proxy_.handler.getSpaceSnapshot(space.id);
      snapshots.set(space.id, this.snapshotView_(snapshot));
    }
    this.snapshots_ = snapshots;
  }

  private snapshotView_(snapshot: SpaceSnapshot|null): SnapshotView {
    if (!snapshot) {
      return {view: buildSidebar([], []), today: []};
    }
    return {
      view: buildSidebar(snapshot.tree, []),
      today: snapshot.today.map(t => ({title: t.title, url: t.url, favicon: t.favicon})),
    };
  }

  // Trackpad swipe over the sidebar (zen:spaces/ZenSpacesSwipe.mjs).
  private onSwipe_(swipe: Swipe) {
    const count = this.spaces_.length;
    if (count < 1) {
      return;
    }
    const current = this.currentIndex_;
    switch (swipe.type) {
      case SwipeType.kStart:
        cancelAnimationFrame(this.animationFrame_);
        this.swipe_ = new SwipeState();
        this.swipeOffset_ = 0;
        // Past the first space the Library, past the last a new space.
        this.edgeAction_ = null;
        this.edgeAllowed_ = {
          library: current === 0 && !this.libraryOpen_,
          add: current === count - 1 && !this.editor_,
        };
        this.loadSnapshots_();
        break;
      case SwipeType.kUpdate: {
        if (!this.swipe_) {
          return;
        }
        // Strips follow the fingers: towards the next space they move left.
        this.swipeOffset_ = -this.swipe_.update(swipe.amount, this.offsetWidth);
        const offset = this.swipeOffset_;
        if (!this.edgeAction_ && offset > 0 && this.edgeAllowed_.library) {
          this.edgeAction_ = 'library';
          this.libraryShown_ = true;
        } else if (!this.edgeAction_ && offset < 0 && this.edgeAllowed_.add) {
          this.edgeAction_ = 'add';
        }
        if (this.edgeAction_ === 'library') {
          this.libraryDrag_ = Math.min(1, Math.max(0, offset / this.offsetWidth * 2));
          return;
        }
        if (this.edgeAction_ === 'add') {
          this.addProgress_ = addSwipeProgress(swipe.amount);
          this.stripX_ = this.spaces_.map(
              (_, i) => circularDiff(i, current, count) * 100 +
                  this.addProgress_ * ADD_SWIPE_SHIFT);
          return;
        }
        if (count < 2) {
          return;
        }
        this.stripX_ = this.spaces_.map(
            (_, i) => circularDiff(i, current, count) * 100 + offset / 2);
        this.setTint_(
            this.colorOf_(current),
            this.colorOf_(neighbourIndex(current, count, offset)),
            Math.min(1, Math.abs(offset) / 200));
        break;
      }
      case SwipeType.kEnd: {
        this.swipe_ = null;
        const action = this.edgeAction_;
        this.edgeAction_ = null;
        if (action === 'library') {
          const open = swipe.direction === -1 || this.libraryDrag_ >= 0.5;
          this.libraryDrag_ = -1;
          open ? this.openLibrary_() : this.closeLibrary_();
          return;
        }
        if (action === 'add') {
          if (this.addProgress_ >= 1) {
            this.onAddSpaceClick_();
          }
          this.addProgress_ = 0;
          this.animateTo_(current);
          return;
        }
        if (count < 2) {
          return;
        }
        if (swipe.direction === 0) {
          this.animateTo_(current);
          return;
        }
        const target = (current + swipe.direction + count) % count;
        this.animateTo_(target).then(() => this.finishSwitch_(target));
        break;
      }
    }
  }

  // Hotkeys, the wheel and the switcher: the same animation without fingers.
  private async switchTo_(id: string) {
    const target = this.spaces_.findIndex(s => s.id === id);
    if (target < 0 || target === this.currentIndex_) {
      return;
    }
    await this.loadSnapshots_();
    await this.animateTo_(target);
    this.finishSwitch_(target);
  }

  // Zen's #animateTabs: a spring without bounce over 250 ms; only the strips
  // on the way between the two spaces move.
  private animateTo_(target: number): Promise<void> {
    cancelAnimationFrame(this.animationFrame_);
    const count = this.spaces_.length;
    const current = this.currentIndex_;
    const from = [...this.stripX_];
    const to = this.spaces_.map((_, i) => circularDiff(i, target, count) * 100);
    const moving = this.spaces_.map(
        (_, i) => target === current || isAnimatedStrip(i, current, target, count));
    const progress = spring(0, 1, SWITCH_DURATION);
    const tintFrom = [...this.tintRgb_];
    const tintTo = this.colorOf_(target);
    const start = performance.now();
    return new Promise(resolve => {
      let done = false;
      const finish = () => {
        if (done) {
          return;
        }
        done = true;
        cancelAnimationFrame(this.animationFrame_);
        this.stripX_ = to;
        this.setTint_(tintFrom, tintTo, 1);
        resolve();
      };
      const step = (now: number) => {
        const t = (now - start) / 1000;
        if (t >= SWITCH_DURATION + 0.05) {
          finish();
          return;
        }
        const p = progress(t);
        this.stripX_ = from.map((x, i) => moving[i] ? x + (to[i]! - x) * p : to[i]!);
        this.setTint_(tintFrom, tintTo, p);
        this.animationFrame_ = requestAnimationFrame(step);
      };
      this.animationFrame_ = requestAnimationFrame(step);
      // Zen's safety net (issue 9334): frames may stop, e.g. in a hidden or
      // occluded window; the switch still completes.
      setTimeout(finish, (SWITCH_DURATION + 0.05) * 1000);
    });
  }

  private finishSwitch_(target: number) {
    const space = this.spaces_[target];
    if (!space) {
      return;
    }
    // The target space's window takes this one's place; this window is hidden
    // and waits at rest for the next time it is shown.
    this.proxy_.handler.switchToSpace(space.id);
    setTimeout(() => this.rest_(), 300);
  }

  protected onSpaceClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['space'];
    if (id) {
      this.switchTo_(id);
    }
  }

  protected iconMask_(icon: string): string {
    return iconMask(icon);
  }

  protected iconText_(icon: string): string {
    return iconMask(icon) ? '' : icon;
  }

  protected onLibraryClick_() {
    this.libraryOpen_ ? this.closeLibrary_() : this.openLibrary_();
  }

  protected onLibraryClose_() {
    this.closeLibrary_();
  }

  protected libraryStyle_(): string {
    const reveal = this.libraryDrag_ >= 0 ? this.libraryDrag_ :
                                            (this.libraryOpen_ ? 1 : 0);
    return `transform: translateX(${(reveal - 1) * 100}%)`;
  }

  protected addSwipeArc_(): string {
    return `${easeInOut(this.addProgress_) * 100} 100`;
  }

  protected addSwipeStyle_(): string {
    const entry = Math.min(this.addProgress_ / ADD_SWIPE_ENTRY, 1);
    return `translate: ${(1 - entry) * ADD_SWIPE_TRAVEL}% 0; opacity: ${entry}`;
  }

  private openLibrary_() {
    clearTimeout(this.libraryTimer_);
    this.libraryShown_ = true;
    this.libraryOpen_ = true;
  }

  private closeLibrary_() {
    this.libraryOpen_ = false;
    clearTimeout(this.libraryTimer_);
    // After the slide out (library.css).
    this.libraryTimer_ = setTimeout(() => this.libraryShown_ = false, 250);
  }

  protected async onAddSpaceClick_() {
    this.editor_ = {id: '', name: '', icon: '', pickerOpen: false};
    this.remoteSpaces_ = [];
    this.remoteSpaces_ = (await this.proxy_.handler.findICloudSpaces()).spaces;
  }

  protected onSpaceConnect_(e: CustomEvent<ICloudSpace>) {
    this.editor_ = null;
    this.proxy_.handler.connectICloudSpace(e.detail);
  }

  private async openICloud_(space: Space) {
    const {enabled, active} =
        await this.proxy_.handler.getICloudSync(space.id);
    this.icloud_ = {space, enabled, active};
  }

  protected icloudState_(): string {
    const card = this.icloud_;
    if (!card) {
      return '';
    }
    if (card.enabled === card.active) {
      return card.enabled ? strings.icloudOn : strings.icloudOff;
    }
    return card.enabled ? strings.icloudOnAfterRestart :
                          strings.icloudOffAfterRestart;
  }

  protected onICloudToggleClick_() {
    const card = this.icloud_;
    if (!card) {
      return;
    }
    this.proxy_.handler.setICloudSync(card.space.id, !card.enabled);
    this.icloud_ = {...card, enabled: !card.enabled};
  }

  protected onRestartClick_() {
    this.proxy_.handler.restartBrowser();
  }

  protected onICloudCloseClick_() {
    this.icloud_ = null;
  }

  protected onSpaceMenuClick_(e: MouseEvent) {
    const space = this.spaces_.find(s => s.current);
    if (space) {
      this.openSpaceMenu_(space, e.clientX, e.clientY);
    }
  }

  protected onSpaceContextmenu_(e: MouseEvent) {
    e.preventDefault();
    const id = (e.currentTarget as HTMLElement).dataset['space'];
    const space = this.spaces_.find(s => s.id === id);
    if (space) {
      this.openSpaceMenu_(space, e.clientX, e.clientY);
    }
  }

  protected onSpaceNameDblclick_() {
    const space = this.spaces_.find(s => s.current);
    if (space) {
      this.editSpace_(space, false);
    }
  }

  // Zen's space context menu (zen-workspaces-panel-*).
  private openSpaceMenu_(space: Space, x: number, y: number) {
    this.menu_ = {
      x,
      y,
      items: [
        {label: strings.editSpace, action: () => this.editSpace_(space, false)},
        {label: strings.changeIcon, action: () => this.editSpace_(space, true)},
        {
          label: strings.editTheme,
          action: () => this.themeEditor_ = {
            space,
            theme: themeFromJson(space.theme),
          },
        },
        {
          label: strings.sleepSpace,
          action: () => this.proxy_.handler.sleepSpace(space.id),
        },
        {label: strings.icloudSync, action: () => this.openICloud_(space)},
        ...(this.spaces_.length > 1 ?
                [{
                  label: strings.deleteSpace,
                  action: () => this.deleting_ = space,
                }] :
                []),
      ],
    };
  }

  private editSpace_(space: Space, pickerOpen: boolean) {
    // The first letter stands in for no icon.
    const icon = [...space.name][0]?.toUpperCase() === space.icon ? '' :
                                                                    space.icon;
    this.editor_ = {id: space.id, name: space.name, icon, pickerOpen};
  }

  protected onSpaceEditDone_(e: CustomEvent<SpaceEdit>) {
    const editor = this.editor_;
    this.editor_ = null;
    if (!editor) {
      return;
    }
    const {name, icon} = e.detail;
    if (!editor.id) {
      this.proxy_.handler.createSpace(name, icon);
      return;
    }
    if (name !== editor.name) {
      this.proxy_.handler.renameSpace(editor.id, name);
    }
    if (icon !== editor.icon) {
      this.proxy_.handler.setSpaceIcon(editor.id, icon);
    }
  }

  protected onSpaceEditCancel_() {
    this.editor_ = null;
  }

  protected async onDeleteConfirmClick_() {
    const space = this.deleting_;
    this.deleting_ = null;
    if (space) {
      await this.proxy_.handler.deleteSpace(space.id);
    }
  }

  protected onDeleteCancelClick_() {
    this.deleting_ = null;
  }

  protected onSpaceDragstart_(e: DragEvent) {
    this.draggedSpace_ = (e.currentTarget as HTMLElement).dataset['space']!;
    e.dataTransfer!.effectAllowed = 'move';
  }

  protected onSpaceDragover_(e: DragEvent) {
    if (this.draggedSpace_) {
      e.preventDefault();
    }
  }

  protected onSpaceDrop_(e: DragEvent) {
    e.preventDefault();
    const target = (e.currentTarget as HTMLElement).dataset['space']!;
    const ids = moveSpace(
        this.spaces_.map(s => s.id), this.draggedSpace_, target);
    this.draggedSpace_ = '';
    if (ids) {
      this.proxy_.handler.reorderSpaces(ids);
    }
  }

  // Before the space list arrives, one strip: this window's own.
  protected get stripSpaces_(): Space[] {
    return this.spaces_.length ? this.spaces_ :
        [{id: '', name: '', icon: '', color: 0, current: true, theme: ''}];
  }

  protected stripStyle_(index: number): string {
    const space = this.stripSpaces_[index];
    const theme = this.themePreview_ && space?.id === this.themePreview_.id ?
        this.themePreview_.theme :
        themeFromJson(space?.theme ?? '');
    return `transform: translateX(${this.stripX_[index] ?? 0}%); ` +
        `background: ${gradientOf(theme)}; --grain: ${theme.texture}`;
  }

  protected onThemeChange_(e: CustomEvent<SpaceTheme>) {
    if (this.themeEditor_) {
      // Lit sets object properties again on every render: the editor's own
      // theme has to be the one bound.
      this.themeEditor_ = {...this.themeEditor_, theme: e.detail};
      this.themePreview_ = {id: this.themeEditor_.space.id, theme: e.detail};
    }
  }

  protected onThemeDone_(e: CustomEvent<SpaceTheme>) {
    const editor = this.themeEditor_;
    this.themeEditor_ = null;
    if (editor) {
      this.proxy_.handler.setSpaceTheme(
          editor.space.id, JSON.stringify(e.detail));
    }
  }

  protected snapshotOf_(id: string): SnapshotView {
    return this.snapshots_.get(id) ?? {view: buildSidebar([], []), today: []};
  }

  protected snapshotFavicon_(url: string, favicon: string): string {
    return favicon ? `url("${favicon}")` : getFaviconForPageURL(url, false);
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

  // Zen's reset button (ZenPinnedTabManager _onTabResetPinButton): resets and
  // selects the tab; with Cmd the page first moves to an ordinary tab.
  protected onResetClick_(e: MouseEvent) {
    e.stopPropagation();
    const id = this.idOf_(e);
    if (!id) {
      return;
    }
    if (e.metaKey) {
      this.proxy_.handler.separateItem(id);
    } else {
      this.proxy_.handler.resetItem(id);
      this.proxy_.handler.openItem(id);
    }
  }

  protected onResetMousemove_(e: MouseEvent) {
    this.resetAccel_ = e.metaKey;
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
        items.push(
            {
              label: row.essential ? strings.resetEssential :
                                     strings.resetPinned,
              action: () => h.resetItem(row.id),
            },
            {
              label: strings.replaceWithCurrent,
              action: () => h.setHomeToCurrent(row.id),
            });
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
      items: [
        {label: strings.newFolder, action: () => this.createFolder_('')},
        {
          label: strings.hideSidebar,
          action: () => this.proxy_.handler.toggleCompactMode(),
        },
        {
          label: strings.importFromArc,
          action: () => this.proxy_.tabStrip
                            .createTabAt(null, 'chrome://cromarc-import/')
                            .catch(() => {}),
        },
      ],
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
    if (changed.has('menu_' as keyof SidebarAppElement) && this.menu_) {
      this.fitMenu_();
    }
  }

  // The sidebar is narrow: a menu opened near its edge moves back inside.
  private fitMenu_() {
    const menu = this.shadowRoot.querySelector<HTMLElement>('#menu');
    if (!menu) {
      return;
    }
    const margin = 4;
    const {width, height} = menu.getBoundingClientRect();
    const x = Math.max(
        margin, Math.min(this.menu_!.x, window.innerWidth - width - margin));
    const y = Math.max(
        margin, Math.min(this.menu_!.y, window.innerHeight - height - margin));
    menu.style.left = `${x}px`;
    menu.style.top = `${y}px`;
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
    this.dropZone_ = '';
  }

  protected onZoneDragleave_(e: DragEvent) {
    if (!(e.currentTarget as Node).contains(e.relatedTarget as Node | null)) {
      this.dropZone_ = '';
    }
  }

  // Dropped on the space header: pinned, or moved, to the top of the tree.
  protected onHeaderDragover_(e: DragEvent) {
    if (this.drag_) {
      e.preventDefault();
      this.dropZone_ = 'header';
    }
  }

  protected onHeaderDrop_(e: DragEvent) {
    e.preventDefault();
    const drag = this.drag_;
    this.onDragend_();
    if (!drag) {
      return;
    }
    if (drag.kind === 'tab') {
      this.proxy_.handler.pinTab(drag.id, '', 0, false);
    } else {
      this.proxy_.handler.setEssential(drag.id, false);
      this.proxy_.handler.moveItem(drag.id, '', 0);
    }
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
      this.dropZone_ = 'end';
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
