// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {DownloadCommand, SwipeType} from '../sidebar.mojom-webui.js';
import type {Download, ICloudSpace, Space, SpaceSnapshot, Swipe, TabLifecycle, TreeNode} from '../sidebar.mojom-webui.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {TabNetworkState} from '/tab_strip_api/tabs.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import './folder_icon.js';
import './library.js';
import './space_editor.js';
import './theme_editor.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {buildSidebar, cellAt, dropTarget, essentialSlot, MAX_ESSENTIALS, pageTitleFor} from './sidebar_model.js';
import type {SpaceEdit} from './space_editor.js';
import {setTestMenuChoice, showMenu, testMenuChoice} from './menu.js';
import type {MenuItem} from './menu.js';
import {iconMask} from './space_icon.js';
import {themeFromJson} from './theme_model.js';
import type {SpaceTheme} from './theme_model.js';
import {canRetry, hasFile, hasProgress, isPending, progressOf, stackEntries, statusText} from './downloads_model.js';
import {downloadsStore} from './downloads_store.js';
import {libraryStrings} from './library_strings.js';
import {LIBRARY_SPRING, LIBRARY_WIDTH_DURATION, libraryWidthEasing, physicalSpring} from './motion.js';
import {ADD_SWIPE_ENTRY, ADD_SWIPE_SHIFT, ADD_SWIPE_TRAVEL, addSwipeProgress, circularDiff, easeInOut, isAnimatedStrip, librarySwipeProgress, moveSpace, neighbourIndex, spring, SWITCH_DURATION, SwipeState} from './spaces.js';
import type {DropPosition, PinnedRow, SidebarView, TreeRow} from './sidebar_model.js';
import {strings} from './strings.js';
import {TabListModel} from './tab_list_model.js';

const PINS_COLLAPSED_KEY = 'cromarc.pinsCollapsed';
// zen-library-widget.css registers --zen-library-progress with @property so
// the download badge's flight transitions; a shadow root ignores @property.
try {
  CSS.registerProperty({
    name: '--stack-progress',
    syntax: '<number>',
    inherits: true,
    initialValue: '0',
  });
} catch {
  // Registered already.
}
// zen-library.css: the Library is at least 418px wide, its rail 110px of it.
const LIBRARY_WIDTH = 418;

// The space form open over the strip: a new space, or `id`'s settings.
export interface SpaceEditorState {
  id: string;
  name: string;
  icon: string;
  pickerOpen: boolean;
}


// What is being dragged: a tree item (pinned or folder) or a Today tab.
interface DragSource {
  kind: 'item'|'tab';
  id: string;
  folder: boolean;
  // Dragged out of the Essentials grid.
  essential: boolean;
  title: string;
  // CSS image of its icon.
  favicon: string;
  // The grabbed point, from the dragged element's top left corner.
  grabX: number;
  grabY: number;
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
      renaming_: {type: String},
      drop_: {type: Object},
      dropZone_: {type: String},
      essentialsSlot_: {type: Number},
      promoOver_: {type: Boolean},
      resetAccel_: {type: Boolean},
      strings: {type: Object},
      minutesLeft_: {type: Object},
      cleared_: {type: Array},
      spaces_: {type: Array},
      snapshots_: {type: Object},
      stripX_: {type: Array},
      editor_: {type: Object},
      deleting_: {type: Object},
      icloud_: {type: Object},
      themeEditor_: {type: Object},
      remoteSpaces_: {type: Array},
      libraryShown_: {type: Boolean},
      libraryOpen_: {type: Boolean},
      libraryDrag_: {type: Number},
      libraryProgress_: {type: Number},
      libraryWidth_: {type: Number},
      primary_: {type: String},
      stack_: {type: Array},
      stackOpen_: {type: Boolean},
      stackClosing_: {type: Boolean},
      stackBadge_: {type: Boolean},
      addProgress_: {type: Number},
      pinsCollapsed_: {type: Boolean},
    };
  }

  protected accessor tabs_: Tab[] = [];
  protected accessor tree_: TreeNode[] = [];
  protected accessor view_: SidebarView =
      {essentials: [], rows: [], attention: [], today: []};
  // Id of the item whose title is being edited.
  protected accessor renaming_: string = '';
  protected accessor drop_: DropIndicator|null = null;
  // The space header or the end of the tree under a drag.
  protected accessor dropZone_: ''|'header'|'end' = '';
  // Where in the Essentials grid the drag would land, -1 before it has been
  // over the grid. As Zen's fake essential tile, it stays until the drag ends.
  protected accessor essentialsSlot_: number = -1;
  // The drag is over Zen's "Add to Essentials" promo.
  protected accessor promoOver_: boolean = false;
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
  // The Library: rendered while open, dragged in or closing.
  protected accessor libraryShown_: boolean = false;
  protected accessor libraryOpen_: boolean = false;
  // How far a swipe has pulled the Library in, 0–1; -1 when no swipe does.
  // How far open the Library is while a swipe moves it.
  protected accessor libraryDrag_: number|null = null;
  // How far out the Library is (Zen's openProgress), past 1 as its spring
  // overshoots.
  protected accessor libraryProgress_: number = 0;
  // Zen's Library width: at least 418px, wider for its Spaces section.
  protected accessor libraryWidth_: number = LIBRARY_WIDTH;
  // The space's primary colour, for the Library's icons.
  protected accessor primary_: string = '';
  // Zen's download stack on the library button (ZenLibraryWidget): the
  // downloads, oldest first, the list over the foot, and the badge.
  protected accessor stack_: Download[] = [];
  protected accessor stackOpen_: boolean = false;
  protected accessor stackClosing_: boolean = false;
  protected accessor stackBadge_: boolean = false;
  // Zen's add-space swipe past the last space: 1 is ready.
  protected accessor addProgress_: number = 0;
  // Zen's collapsible pins: the space header folds the pinned tabs away.
  protected accessor pinsCollapsed_: boolean = false;
  private pinnedCount_ = -1;
  private treeLoaded_ = false;
  // What a swipe at the edge of the spaces does (zen ZenSpacesSwipe actions).
  private edgeAction_: 'library'|'add'|null = null;
  private edgeAllowed_ = {library: false, add: false};
  private libraryBefore_ = 0;
  // The space whose window is being made ready for a switch, -1 for none.
  private prepared_ = -1;
  private libraryAnimation_ = 0;
  // Bumped by every Library animation, so a stale one's safety net does
  // nothing.
  private libraryRun_ = 0;
  private libraryWidthAnimation_ = 0;
  // The sidebar's width as the Library came out: the sidebar keeps it under
  // the Library while the column widens.
  private toolboxWidth_ = 0;
  // A tab opening closes the Library (Zen's onTabOpen), but not one it opens
  // in the background itself (keepOpenWhile).
  private keepLibraryOpenUntil_ = 0;
  private stackCloseTimer_ = 0;
  // A download added since the list came in shows the badge until the
  // stack is next looked at.
  private stackSeen_: Set<number>|null = null;
  private stackRecent_ = false;
  private deferred_: Set<string> = new Set();
  private clearedTimer_ = 0;
  private swipe_: SwipeState|null = null;
  // A switch animating, and a space list that came in meanwhile.
  private moving_ = false;
  private pendingSpaces_: Space[]|null = null;
  private swipeOffset_ = 0;
  private animationFrame_ = 0;

  protected accessor strings: typeof strings = strings;
  private readonly proxy_ = BrowserProxy.getInstance();
  // Bumped on every TabStripService snapshot; older observers go quiet.
  private generation_ = 0;
  private drag_: DragSource|null = null;
  // Untitled pinned items already given their page's title.
  private readonly titled_: Set<string> = new Set();
  // The page's drag image (see #drag-ghost in app.css): its row and tile
  // forms, and which one shows.
  private ghostRow_: HTMLElement|null = null;
  private ghostTile_: HTMLElement|null = null;
  private ghostIsTile_ = false;
  // Hides the native drag image.
  private readonly blankImage_ = Object.assign(new Image(), {
    src: 'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7',
  });
  // Essentials' positions before a render that moves them.
  private tileRects_: Map<string, DOMRect>|null = null;
  // CDP tests can't click a native menu: they name the item to choose here
  // before opening the menu.
  // For browser tests (menu.ts).
  set testMenuChoice(choice: string|null) {
    setTestMenuChoice(choice);
  }
  get testMenuChoice(): string|null {
    return testMenuChoice();
  }

  override connectedCallback() {
    super.connectedCallback();
    try {
      this.pinsCollapsed_ = localStorage.getItem(PINS_COLLAPSED_KEY) === '1';
    } catch {
      // No storage: the pinned tabs start open.
    }
    this.proxy_.callbackRouter.onTreeChanged.addListener(
        (nodes: TreeNode[]) => this.setTree_(nodes));
    this.proxy_.handler.getTree().then(({nodes}) => this.setTree_(nodes));
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
    downloadsStore().subscribe(downloads => this.onDownloads_(downloads));
    this.addEventListener('dragover', e => this.moveGhost_(e));
    this.addEventListener('dragleave', e => {
      // Out of the page: nothing there shows the drag image.
      if (!e.relatedTarget && this.drag_) {
        this.showGhost_(null);
      }
    });
  }

  override willUpdate(changed: PropertyValues<this>) {
    super.willUpdate(changed);
    if (changed.has('tabs_' as keyof SidebarAppElement) ||
        changed.has('tree_' as keyof SidebarAppElement) ||
        changed.has('minutesLeft_' as keyof SidebarAppElement)) {
      this.view_ = buildSidebar(this.tree_, this.tabs_, this.deferred_);
      this.fillTitles_();
      this.checkPins_();
    }
    if (this.drag_ &&
        changed.has('essentialsSlot_' as keyof SidebarAppElement)) {
      this.tileRects_ = new Map(
          [...this.tiles_()].map(([id, tile]) => [id, tile.getBoundingClientRect()]));
    }
  }

  private setTree_(nodes: TreeNode[]) {
    this.treeLoaded_ = true;
    this.tree_ = nodes;
  }

  // As Zen's space (checkPinsExistence, TabPinned): no pinned tabs, nothing
  // to fold; pinning one unfolds them.
  private checkPins_() {
    if (!this.treeLoaded_) {
      return;
    }
    const count = this.view_.rows.filter(row => row.kind === 'pinned').length;
    if (this.pinsCollapsed_ &&
        (count === 0 || (this.pinnedCount_ >= 0 && count > this.pinnedCount_))) {
      this.setPinsCollapsed_(false);
    }
    this.pinnedCount_ = count;
  }

  private setPinsCollapsed_(collapsed: boolean) {
    this.pinsCollapsed_ = collapsed;
    try {
      localStorage.setItem(PINS_COLLAPSED_KEY, collapsed ? '1' : '0');
    } catch {
      // Not kept across restarts then.
    }
  }

  // Zen's indicator click (ZenSpace.mjs): folds or unfolds the pinned tabs.
  protected onHeaderClick_(e: MouseEvent) {
    if (e.button !== 0 || this.view_.rows.length === 0 ||
        (e.target as Element).closest('button')) {
      return;
    }
    this.setPinsCollapsed_(!this.pinsCollapsed_);
  }

  // Folded, the pinned tabs keep only the selected one, as a folded folder.
  protected visibleRows_(): TreeRow[] {
    return this.pinsCollapsed_ ?
        this.view_.rows.filter(
            row => row.kind === 'pinned' && this.isActive_(row.tab)) :
        this.view_.rows;
  }

  // Pinned items imported from Arc's export file come untitled: each keeps
  // its page's title once it has loaded.
  private fillTitles_() {
    for (const row of [...this.view_.essentials, ...this.view_.rows]) {
      const title = row.kind === 'pinned' ?
          pageTitleFor(row, row.tab?.networkState === TabNetworkState.kNone) :
          '';
      if (title && !this.titled_.has(row.id)) {
        this.titled_.add(row.id);
        this.proxy_.handler.renameItem(row.id, title);
      }
    }
  }

  // Spaces.

  protected get currentIndex_(): number {
    return Math.max(0, this.spaces_.findIndex(s => s.current));
  }

  private setSpaces_(spaces: Space[]) {
    // Mid-swipe or mid-switch only how the spaces look changes (a space's
    // profile loading brings its icon); a new order waits for the strips to
    // rest.
    if (this.swipe_ || this.moving_) {
      const ids = (list: Space[]) => list.map(s => s.id).join('\n');
      if (ids(spaces) === ids(this.spaces_)) {
        this.spaces_ = spaces;
      } else {
        this.pendingSpaces_ = spaces;
      }
      return;
    }
    this.pendingSpaces_ = null;
    this.spaces_ = spaces;
    this.snapshots_ = new Map();
    this.rest_();
  }

  // Back at this window's space, unless a swipe or switch has started since.
  private settle_() {
    if (!this.swipe_ && !this.moving_) {
      this.rest_();
    }
  }

  // Every strip in its place for this window's own space.
  private rest_() {
    if (this.pendingSpaces_) {
      this.setSpaces_(this.pendingSpaces_);
      return;
    }
    cancelAnimationFrame(this.animationFrame_);
    const current = this.currentIndex_;
    const count = this.spaces_.length;
    this.stripX_ = this.spaces_.map(
        (_, i) => circularDiff(i, current, count) * 100);
    this.blendBackground_(current, 0);
    this.updatePrimary_();
  }

  // The window of space `index` gets ready under this one for the coming
  // swap (SpaceController::PrepareSwitch); -1 puts it away.
  private prepare_(index: number) {
    if (index === this.prepared_) {
      return;
    }
    this.prepared_ = index;
    const space = index >= 0 && index !== this.currentIndex_ ?
        this.spaces_[index] :
        undefined;
    this.proxy_.handler.prepareSpaceSwitch(space?.id ?? '');
  }

  // Zen fades the background between the two spaces' themes as the strips
  // move (--zen-background-opacity = 1 - |offset| / 200); the browser paints
  // it under the whole window.
  private blendBackground_(target: number, progress: number) {
    const theme = this.spaces_[target]?.theme ?? '';
    this.proxy_.handler.blendSpaceTheme(theme, progress);
  }

  // Zen's --zen-primary-color: the theme's first colour, the accent without
  // one (ZenGradientGenerator).
  private updatePrimary_() {
    const space = this.spaces_[this.currentIndex_];
    const primary = themeFromJson(space?.theme ?? '').colors[0];
    if (primary) {
      this.style.setProperty('--zen-primary-color', `rgb(${primary.join(', ')})`);
    } else {
      this.style.removeProperty('--zen-primary-color');
    }
    this.primary_ = primary ? primary.join(',') : '';
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
        // Past the first space the Library, past the last a new space; with
        // the Library open, every swipe moves the Library.
        this.edgeAction_ = this.libraryOpen_ ? 'library' : null;
        this.libraryBefore_ = this.libraryOpen_ ? 1 : 0;
        if (this.libraryOpen_) {
          cancelAnimationFrame(this.libraryAnimation_);
          this.libraryRun_++;
        }
        this.edgeAllowed_ = {
          library: this.libraryOpen_ || current === 0,
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
        } else if (!this.edgeAction_ && offset < 0 && this.edgeAllowed_.add) {
          this.edgeAction_ = 'add';
        }
        if (this.edgeAction_ === 'library') {
          this.libraryDrag_ = librarySwipeProgress(
              this.libraryBefore_, offset, this.offsetWidth);
          this.setLibraryProgress_(this.libraryDrag_);
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
        if (offset !== 0) {
          this.prepare_(neighbourIndex(current, count, offset));
        }
        this.blendBackground_(neighbourIndex(current, count, offset),
                              Math.min(1, Math.abs(offset) / 200));
        break;
      }
      case SwipeType.kEnd: {
        this.swipe_ = null;
        const action = this.edgeAction_;
        this.edgeAction_ = null;
        if (action) {
          this.prepare_(-1);
        }
        if (action === 'library') {
          // Zen's stopSwipe: the way the swipe went; a swipe that fell short
          // settles on the nearer end.
          const open = swipe.direction !== 0 ? swipe.direction === -1 :
                                               (this.libraryDrag_ ?? 0) >= 0.5;
          this.libraryDrag_ = null;
          this.animateLibrary_(open ? 1 : 0);
          return;
        }
        if (action === 'add') {
          if (this.addProgress_ >= 1) {
            this.onAddSpaceClick_();
          }
          this.addProgress_ = 0;
          this.animateTo_(current).then(() => this.settle_());
          return;
        }
        if (count < 2) {
          return;
        }
        if (swipe.direction === 0) {
          this.prepare_(-1);
          this.animateTo_(current).then(() => this.settle_());
          return;
        }
        const target = (current + swipe.direction + count) % count;
        this.prepare_(target);
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
    this.prepare_(target);
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
    const start = performance.now();
    this.moving_ = true;
    return new Promise(resolve => {
      let done = false;
      const finish = () => {
        if (done) {
          return;
        }
        done = true;
        this.moving_ = false;
        cancelAnimationFrame(this.animationFrame_);
        this.stripX_ = to;
        this.blendBackground_(target, 1);
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
        this.blendBackground_(target, p);
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
    this.prepared_ = -1;
    setTimeout(() => {
      // Zen's Library belongs to the window on screen, not to a space: it
      // doesn't wait here for the space to come back.
      this.closeLibraryNow_();
      this.rest_();
    }, 300);
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

  protected onLibraryClick_() {
    this.libraryOpen_ ? this.closeLibrary_() : this.openLibrary_();
  }

  protected onLibraryClose_() {
    this.closeLibrary_();
  }

  protected onLibraryKeepOpen_() {
    this.keepLibraryOpenUntil_ = Date.now() + 2000;
  }

  protected onLibraryCloseNow_() {
    this.closeLibraryNow_();
  }

  // The section asks for its width (Zen's --zen-library-content-width); the
  // Library and the column follow it over 0.3s.
  protected onLibraryWidth_(e: CustomEvent<number>) {
    const target = Math.max(LIBRARY_WIDTH, Math.round(e.detail));
    cancelAnimationFrame(this.libraryWidthAnimation_);
    if (target === this.libraryWidth_) {
      return;
    }
    const from = this.libraryWidth_;
    const start = performance.now();
    const step = (now: number) => {
      const t = Math.min(1, (now - start) / LIBRARY_WIDTH_DURATION);
      this.libraryWidth_ =
          Math.round(from + (target - from) * libraryWidthEasing(t));
      if (this.libraryShown_) {
        this.setLibraryProgress_(this.libraryProgress_);
      }
      if (t < 1) {
        this.libraryWidthAnimation_ = requestAnimationFrame(step);
      }
    };
    this.libraryWidthAnimation_ = requestAnimationFrame(step);
  }

  protected onLibrarySpaceMenu_(
      e: CustomEvent<{id: string, x: number, y: number}>) {
    const space = this.spaces_.find(s => s.id === e.detail.id);
    if (space) {
      this.openSpaceMenu_(space, e.detail.x, e.detail.y);
    }
  }

  protected onLibrarySpaceTheme_(e: CustomEvent<string>) {
    const space = this.spaces_.find(s => s.id === e.detail);
    if (space) {
      this.closeLibrary_();
      this.themeEditor_ = {space, theme: themeFromJson(space.theme)};
    }
  }

  protected onLibraryCreateSpace_() {
    this.closeLibrary_();
    this.onAddSpaceClick_();
  }

  protected libraryStyle_(): string {
    // A section wider than Zen's 418px gets no more than the column has.
    const width = this.libraryWidth_ > LIBRARY_WIDTH ?
        `min(${this.libraryWidth_}px, 100vw)` :
        `${this.libraryWidth_}px`;
    return `width: ${width}; transform: translateX(${
        (this.libraryProgress_ - 1) * 100}%)`;
  }

  // Zen's toolbox under the Library: it fades and shrinks a little as the
  // Library comes over it, keeping its width while the column widens.
  protected toolboxStyle_(): string {
    if (!this.libraryShown_) {
      return '';
    }
    const progress = Math.min(1, Math.max(0, this.libraryProgress_) * 3);
    return `width: ${this.toolboxWidth_}px; opacity: ${1 - progress}; ` +
        `scale: ${1 - progress * 0.04}`;
  }

  protected addSwipeArc_(): string {
    return `${easeInOut(this.addProgress_) * 100} 100`;
  }

  protected addSwipeStyle_(): string {
    const entry = Math.min(this.addProgress_ / ADD_SWIPE_ENTRY, 1);
    return `translate: ${(1 - entry) * ADD_SWIPE_TRAVEL}% 0; opacity: ${entry}`;
  }

  private openLibrary_() {
    this.animateLibrary_(1);
  }

  private closeLibrary_() {
    this.animateLibrary_(0);
  }

  // zen:library/ZenLibrary.mjs openProgress: the column takes the Library's
  // width as it comes out, moving the page aside.
  private setLibraryProgress_(progress: number) {
    if (!this.libraryShown_) {
      this.toolboxWidth_ = this.offsetWidth;
      this.libraryShown_ = true;
    }
    this.libraryProgress_ = progress;
    const out = Math.max(0, progress);
    this.proxy_.handler.setLibraryWidth(
        out > 0 ? Math.round(
                      this.toolboxWidth_ +
                      out * (this.libraryWidth_ - this.toolboxWidth_)) :
                  0);
  }

  // ZenLibrary.animateProgress: its spring to open (1) or shut (0).
  private animateLibrary_(target: number) {
    cancelAnimationFrame(this.libraryAnimation_);
    this.libraryOpen_ = target === 1;
    if (target === 0 && !this.libraryShown_) {
      return;
    }
    const spring = physicalSpring(this.libraryProgress_, target, LIBRARY_SPRING);
    const start = performance.now();
    const run = ++this.libraryRun_;
    const finish = () => {
      if (run !== this.libraryRun_) {
        return;
      }
      cancelAnimationFrame(this.libraryAnimation_);
      this.libraryRun_++;
      this.setLibraryProgress_(target);
      if (target === 0) {
        this.libraryShown_ = false;
      }
    };
    const step = (now: number) => {
      const {value, done} = spring((now - start) / 1000);
      if (done) {
        finish();
        return;
      }
      this.setLibraryProgress_(value);
      this.libraryAnimation_ = requestAnimationFrame(step);
    };
    this.libraryAnimation_ = requestAnimationFrame(step);
    // As animateTo_'s: frames stop in a hidden or zero-width view (compact
    // mode's column as it shuts); the Library still gets where it goes.
    setTimeout(finish, 1000);
  }

  // Leaving the space: shut at once, the window being hidden anyway.
  private closeLibraryNow_() {
    cancelAnimationFrame(this.libraryAnimation_);
    this.libraryRun_++;
    this.libraryOpen_ = false;
    if (this.libraryShown_) {
      this.setLibraryProgress_(0);
      this.libraryShown_ = false;
    }
  }

  // A tab opened in the window.
  private onTabOpened_() {
    if (this.libraryOpen_ && Date.now() > this.keepLibraryOpenUntil_) {
      this.closeLibrary_();
    }
  }

  // The download stack (zen:library/ZenLibraryWidget.sys.mjs).

  private onDownloads_(downloads: Download[]) {
    if (this.stackSeen_) {
      if (downloads.some(download => !this.stackSeen_!.has(download.id))) {
        this.stackRecent_ = true;
      }
    }
    this.stackSeen_ = new Set(downloads.map(download => download.id));
    this.stack_ = downloads;
    this.updateStackBadge_();
    if (!downloads.length) {
      this.closeStack_();
    }
  }

  private newestDownload_(): Download|undefined {
    return this.stack_[this.stack_.length - 1];
  }

  private updateStackBadge_() {
    const newest = this.newestDownload_();
    this.stackBadge_ = (!!newest && isPending(newest)) || this.stackRecent_;
  }

  protected stackEntries_(): Array<Download|null> {
    return stackEntries(this.stack_);
  }

  protected newestPending_(): boolean {
    const newest = this.newestDownload_();
    return !!newest && isPending(newest);
  }

  protected downloadPending_(download: Download|null): boolean {
    return !!download && isPending(download);
  }

  protected downloadHasFile_(download: Download|null): boolean {
    return !!download && hasFile(download);
  }

  protected downloadImage_(download: Download|null|undefined): string {
    if (!download) {
      return '';
    }
    const icon = `url("chrome://fileicon/?path=${
        encodeURIComponent(download.path || download.fileName)}&scale=2x")`;
    return `--download-image: ${
        download.previewUrl ? `url("${download.previewUrl}"), ` : ''}${icon}`;
  }

  protected downloadDash_(download: Download|null|undefined): string {
    return `${download && hasProgress(download) ? progressOf(download) : 0} 100`;
  }

  protected downloadStatus_(download: Download|null): string {
    if (!download) {
      return '';
    }
    return isPending(download) && download.openWhenComplete ?
        libraryStrings.openWhenDone :
        statusText(download, Date.now());
  }

  protected downloadActionLabel_(download: Download|null): string {
    return this.downloadPending_(download) ? libraryStrings.cancelDownload :
                                             libraryStrings.showInFinder;
  }

  protected onLibraryButtonMouseenter_() {
    clearTimeout(this.stackCloseTimer_);
    this.openStack_();
  }

  protected onDownloadListMouseenter_() {
    clearTimeout(this.stackCloseTimer_);
  }

  protected onStackMouseleave_() {
    clearTimeout(this.stackCloseTimer_);
    // ZenLibraryWidget's CLOSE_DELAY_MS.
    this.stackCloseTimer_ = setTimeout(() => this.closeStack_(), 200);
  }

  private openStack_() {
    if (!this.stack_.length || this.stackOpen_ || this.libraryOpen_) {
      return;
    }
    this.aimBadge_();
    this.stackClosing_ = false;
    this.stackOpen_ = true;
  }

  private closeStack_() {
    if (!this.stackOpen_) {
      return;
    }
    this.stackRecent_ = false;
    this.updateStackBadge_();
    this.stackOpen_ = false;
    this.stackClosing_ = true;
    // After the mask's slide back (zen-library-widget.css).
    setTimeout(() => {
      if (!this.stackOpen_) {
        this.stackClosing_ = false;
      }
    }, 300);
  }

  // ZenLibraryWidget #aimBadge: the button's badge flies to the newest
  // entry's as the list opens.
  private aimBadge_() {
    const badge = this.shadowRoot.querySelector<HTMLElement>(
        '#library-button-badge');
    const target = this.shadowRoot.querySelector<HTMLElement>(
        '#download-list > .download-entry:last-child .download-badge');
    const entry = target?.closest<HTMLElement>('.download-entry');
    if (!badge || !target || !entry) {
      return;
    }
    const from = badge.getBoundingClientRect();
    const to = target.getBoundingClientRect();
    const transform = getComputedStyle(badge).transform;
    const matrix = new DOMMatrixReadOnly(transform === 'none' ? undefined : transform);
    // The entry is still lowered while closed; aim where it ends.
    const entryTransform = getComputedStyle(entry).transform;
    const rise = entryTransform === 'none' ? 0 :
                                             new DOMMatrixReadOnly(entryTransform).f;
    badge.style.setProperty(
        '--stack-badge-to-x', `${matrix.e + to.left - from.left}px`);
    badge.style.setProperty(
        '--stack-badge-to-y', `${matrix.f + to.top - from.top - rise}px`);
    const list = this.shadowRoot.querySelector<HTMLElement>('#download-list');
    if (list) {
      this.style.setProperty('--stack-height', `${list.offsetHeight}px`);
    }
  }

  private stackDownloadOf_(e: Event): Download|undefined {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    return this.stack_.find(download => download.id === id);
  }

  private doDownload_(download: Download, command: DownloadCommand) {
    this.proxy_.handler.doDownload(download.id, command);
  }

  // ZenLibraryWidget #openDownload: one coming in opens when done, a held
  // back one is asked about in the Library, a finished one opens, a failed
  // one retries, anything else opens its page.
  protected onDownloadEntryClick_(e: MouseEvent) {
    const download = this.stackDownloadOf_(e);
    if (!download || e.button !== 0) {
      return;
    }
    if (isPending(download) && !download.dangerous) {
      this.doDownload_(download, DownloadCommand.kOpen);
    } else if (download.dangerous) {
      this.closeStack_();
      this.openLibrary_();
    } else if (hasFile(download)) {
      this.doDownload_(download, DownloadCommand.kOpen);
    } else if (canRetry(download)) {
      this.doDownload_(download, DownloadCommand.kRetry);
    } else if (download.url) {
      this.proxy_.handler.openUrl(download.url, false);
    }
  }

  // The entry's one button calls off a download on its way, and shows the
  // file once it is here.
  protected onDownloadActionClick_(e: MouseEvent) {
    e.stopPropagation();
    const download = this.stackDownloadOf_(e);
    if (!download) {
      return;
    }
    if (isPending(download)) {
      this.doDownload_(download, DownloadCommand.kCancel);
    } else if (hasFile(download)) {
      this.doDownload_(download, DownloadCommand.kShow);
    }
  }

  // ZenLibraryWidget #contextMenuItems.
  protected onDownloadEntryContextmenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const download = this.stackDownloadOf_(e);
    if (!download) {
      return;
    }
    const s = libraryStrings;
    const active = isPending(download);
    const file = hasFile(download);
    const items: MenuItem[] = [];
    if (active && !download.paused) {
      items.push({
        label: s.menuPause,
        action: () => this.doDownload_(download, DownloadCommand.kPause),
      });
    } else if (active) {
      items.push({
        label: s.menuResume,
        action: () => this.doDownload_(download, DownloadCommand.kResume),
      });
    } else if (canRetry(download)) {
      items.push({
        label: s.retryDownload,
        action: () => this.doDownload_(download, DownloadCommand.kRetry),
      });
    }
    if (file) {
      items.push({
        label: s.showInFinder,
        action: () => this.doDownload_(download, DownloadCommand.kShow),
      });
    }
    if (download.url) {
      items.push(
          {
            label: s.menuGoToPage,
            action: () => this.proxy_.handler.openUrl(download.url, false),
          },
          {
            label: s.menuCopyLink,
            action: () => this.doDownload_(download, DownloadCommand.kCopyLink),
          });
    }
    items.push({label: ''});
    if (file) {
      items.push({
        label: s.menuDelete,
        action: () => this.doDownload_(download, DownloadCommand.kDeleteFile),
      });
    }
    if (!active) {
      items.push({
        label: s.menuRemoveFromHistory,
        action: () => this.doDownload_(download, DownloadCommand.kHide),
      });
    }
    if (this.stack_.some(other => !isPending(other))) {
      items.push({
        label: s.menuClearPreview,
        action: () => this.proxy_.handler.clearDownloads(),
      });
    }
    this.showMenu_(e.clientX, e.clientY, items).then(
        () => this.onStackMouseleave_());
  }

  protected onDownloadEntryDragstart_(e: DragEvent) {
    e.preventDefault();
    const download = this.stackDownloadOf_(e);
    if (download && hasFile(download)) {
      this.doDownload_(download, DownloadCommand.kDrag);
    }
  }

  // Zen's "+" at the bottom (zenCreateNewPopup); split view isn't here.
  protected onCreateNewClick_(e: MouseEvent) {
    const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
    this.showMenu_(rect.left, rect.top, [
      {label: strings.newSpace, action: () => this.onAddSpaceClick_()},
      {label: strings.createFolder, action: () => this.createFolder_('')},
      {label: '', action: () => {}},
      {label: strings.newTab, action: () => this.onNewTabClick_()},
    ]);
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
    // What comes up in the sidebar comes up with the Library out of the way
    // (the Library's Spaces opens this menu too).
    const inSidebar = (show: () => void) => () => {
      if (this.libraryOpen_) {
        this.closeLibrary_();
      }
      show();
    };
    this.showMenu_(x, y, [
        {
          label: strings.editSpace,
          action: inSidebar(() => this.editSpace_(space, false)),
        },
        {
          label: strings.changeIcon,
          action: inSidebar(() => this.editSpace_(space, true)),
        },
        {
          label: strings.editTheme,
          action: inSidebar(() => this.themeEditor_ = {
            space,
            theme: themeFromJson(space.theme),
          }),
        },
        {
          label: strings.sleepSpace,
          action: () => this.proxy_.handler.sleepSpace(space.id),
        },
        {
          label: strings.icloudSync,
          action: inSidebar(() => this.openICloud_(space)),
        },
        ...(this.spaces_.length > 1 ?
                [{
                  label: strings.deleteSpace,
                  action: inSidebar(() => this.deleting_ = space),
                }] :
                []),
    ]);
  }

  private editSpace_(space: Space, pickerOpen: boolean) {
    this.editor_ = {id: space.id, name: space.name, icon: space.icon, pickerOpen};
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

  // The form's "Edit theme": the theme editor takes the form's place.
  protected onSpaceEditTheme_() {
    const space = this.spaces_.find(s => s.id === this.editor_?.id);
    this.editor_ = null;
    if (space) {
      this.themeEditor_ = {space, theme: themeFromJson(space.theme)};
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
    return `transform: translateX(${this.stripX_[index] ?? 0}%)`;
  }

  protected onThemeChange_(e: CustomEvent<SpaceTheme>) {
    if (this.themeEditor_) {
      // Lit sets object properties again on every render: the editor's own
      // theme has to be the one bound.
      this.themeEditor_ = {...this.themeEditor_, theme: e.detail};
      this.proxy_.handler.previewSpaceTheme(JSON.stringify(e.detail));
    }
  }

  protected onThemeDone_(e: CustomEvent<SpaceTheme>) {
    const editor = this.themeEditor_;
    this.themeEditor_ = null;
    if (editor) {
      this.proxy_.handler.setSpaceTheme(
          editor.space.id, JSON.stringify(e.detail));
    }
    this.proxy_.handler.previewSpaceTheme('');
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
      onTabsCreated: event => {
        apply(model.onTabsCreated(event));
        this.onTabOpened_();
      },
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
            action: () => row.essential ?
                h.setEssential(row.id, false) :
                this.addToEssentials_(
                    row.id, 'item', this.view_.essentials.length),
          },
          {label: strings.rename, action: () => this.renaming_ = row.id},
          {label: strings.copyLink, action: () => this.copy_(row.homeUrl)},
          {label: strings.unpin, action: () => h.removeItem(row.id)});
    }
    this.showMenu_(e.clientX, e.clientY, items);
  }

  protected onTodayContextmenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const tab = this.todayOf_(e);
    if (!tab) {
      return;
    }
    const h = this.proxy_.handler;
    this.showMenu_(e.clientX, e.clientY, [
      {label: strings.pin, action: () => h.pinTab(tab.id, '', this.rootSize_(), false)},
      {
        label: strings.addToEssentials,
        action: () =>
            this.addToEssentials_(tab.id, 'tab', this.view_.essentials.length),
      },
      {label: strings.copyLink, action: () => this.copy_(tab.url)},
      {label: strings.close, action: () => this.proxy_.tabStrip.closeNodes([tab.id]).catch(() => {})},
    ]);
  }

  protected onBackgroundContextmenu_(e: MouseEvent) {
    e.preventDefault();
    this.showMenu_(e.clientX, e.clientY, [
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
    ]);
  }

  private showMenu_(x: number, y: number, items: MenuItem[]) {
    return showMenu(x, y, items);
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
    if (this.tileRects_) {
      this.animateTiles_(this.tileRects_);
      this.tileRects_ = null;
    }
  }

  // Drag and drop (Arc): a Today tab dropped into the tree is pinned there, a
  // tree item moves, a tab or pinned item dropped on the Essentials grid
  // becomes an essential (Zen), a pinned item dropped on Today is unpinned.

  protected onDragstart_(e: DragEvent) {
    const el = e.currentTarget as HTMLElement;
    const kind = el.dataset['kind'] === 'tab' ? 'tab' : 'item';
    const id = el.dataset['id'];
    if (!id) {
      return;
    }
    const rect = el.getBoundingClientRect();
    const tab = kind === 'tab' ? this.view_.today.find(t => t.id === id) : null;
    const row = this.view_.rows.find(r => r.id === id);
    const essential = this.view_.essentials.find(r => r.id === id);
    const pinned = essential ?? (row?.kind === 'pinned' ? row : null);
    const drag: DragSource = {
      kind, id,
      folder: row?.kind === 'folder',
      essential: !!essential,
      title: tab ? tab.title || tab.url : (essential ?? row)?.title ?? '',
      favicon: tab ? this.faviconFor_(tab.url, tab) :
          pinned ? this.faviconFor_(pinned.homeUrl, pinned.tab) : '',
      grabX: e.clientX - rect.left,
      grabY: e.clientY - rect.top,
    };
    this.drag_ = drag;
    e.dataTransfer!.effectAllowed = 'move';
    e.dataTransfer!.setData('text/plain', id);
    e.dataTransfer!.setDragImage(this.blankImage_, 0, 0);
    const x = e.clientX;
    const y = e.clientY;
    // Changing the page during dragstart can cancel the drag in Chromium.
    setTimeout(() => {
      if (this.drag_ !== drag) {
        return;
      }
      this.ghostRow_ = this.ghostRowFor_(el, drag, rect);
      this.ghostTile_ = this.ghostTileFor_(el, drag);
      this.showGhost_(essential ? this.ghostTile_ : this.ghostRow_);
      this.placeGhost_(x, y);
      if (essential) {
        // Zen hides the dragged essential and keeps its place open.
        this.essentialsSlot_ = this.view_.essentials.indexOf(essential);
      }
    });
  }

  protected onDragend_() {
    this.drag_ = null;
    this.drop_ = null;
    this.dropZone_ = '';
    this.essentialsSlot_ = -1;
    this.promoOver_ = false;
    this.showGhost_(null);
    this.ghostRow_ = null;
    this.ghostTile_ = null;
  }

  // The drag image (zen:drag-and-drop/ZenDragAndDrop.js): a copy of the
  // dragged row or essential at the grabbed point; over Essentials, a tile.

  private ghostRowFor_(el: HTMLElement, drag: DragSource, rect: DOMRect):
      HTMLElement {
    let row: HTMLElement;
    let width = rect.width;
    let height = rect.height;
    if (drag.essential) {
      // An essential takes the size of a row, as any tab there (Zen,
      // #makeDragImageNonEssential).
      row = document.createElement('div');
      row.className = 'row pinned';
      const favicon = document.createElement('span');
      favicon.className = 'favicon';
      favicon.style.backgroundImage = drag.favicon;
      const title = document.createElement('span');
      title.className = 'title';
      title.textContent = drag.title;
      row.append(favicon, title);
      const tree = this.shadowRoot.querySelector<HTMLElement>('#tree');
      width = tree?.clientWidth ?? width;
      height = parseFloat(getComputedStyle(this).getPropertyValue('--row-height'));
    } else {
      row = this.ghostCopy_(el);
    }
    row.style.width = `${width}px`;
    row.style.height = `${height}px`;
    return row;
  }

  private ghostTileFor_(el: HTMLElement, drag: DragSource): HTMLElement {
    if (drag.essential) {
      const tile = this.ghostCopy_(el);
      tile.classList.remove('dragging');
      tile.style.width = `${el.offsetWidth}px`;
      return tile;
    }
    // Zen's essential look for the dragged tab: selected, its icon only.
    const tile = document.createElement('div');
    tile.className = 'essential';
    tile.toggleAttribute('active', true);
    const favicon = document.createElement('span');
    favicon.className = 'favicon';
    favicon.style.backgroundImage = drag.favicon;
    tile.append(favicon);
    return tile;
  }

  private ghostCopy_(el: HTMLElement): HTMLElement {
    const copy = el.cloneNode(true) as HTMLElement;
    copy.removeAttribute('data-id');
    copy.removeAttribute('draggable');
    return copy;
  }

  private showGhost_(ghost: HTMLElement|null) {
    const host = this.shadowRoot.querySelector<HTMLElement>('#drag-ghost');
    host?.replaceChildren(...(ghost ? [ghost] : []));
    this.ghostIsTile_ = !!ghost && ghost === this.ghostTile_;
  }

  // Over Essentials the dragged tab turns into a tile; it turns back into a
  // row outside them (Zen, #makeDragImageEssential/NonEssential).
  private moveGhost_(e: DragEvent) {
    const drag = this.drag_;
    if (!drag || !this.ghostRow_ || !this.ghostTile_) {
      return;
    }
    const essentials = this.shadowRoot.querySelector('#essentials');
    const overEssentials = !!essentials && this.canBeEssential_(drag) &&
        e.composedPath().includes(essentials);
    const ghost = overEssentials ? this.ghostTile_ : this.ghostRow_;
    if (!ghost.isConnected) {
      this.showGhost_(ghost);
    }
    if (overEssentials && !drag.essential) {
      this.ghostTile_.style.width = `${this.tileWidth_()}px`;
    }
    this.placeGhost_(e.clientX, e.clientY);
  }

  private placeGhost_(x: number, y: number) {
    const drag = this.drag_;
    const host = this.shadowRoot.querySelector<HTMLElement>('#drag-ghost');
    if (!drag || !host) {
      return;
    }
    // A tab turned into a tile hangs off the cursor (Zen's -16, -16 drag
    // image offset); anything else stays where it was grabbed.
    const tile = this.ghostIsTile_ && !drag.essential;
    const left = tile ? x + 16 : x - drag.grabX;
    const top = tile ? y + 16 : y - drag.grabY;
    host.style.transform = `translate(${left}px, ${top}px)`;
  }

  // Zen, #essentialTileRect: the slot's width, or with none yet, a tile as
  // the grid would lay it out.
  private tileWidth_(): number {
    const essentials = this.shadowRoot.querySelector<HTMLElement>('#essentials');
    const slot = essentials?.querySelector<HTMLElement>('.essential.slot');
    if (slot) {
      return slot.offsetWidth;
    }
    const height =
        parseFloat(getComputedStyle(this).getPropertyValue('--essential-height'));
    return Math.max((essentials?.clientWidth ?? 0) * .237, height + 4);
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
      return;
    }
    // An essential dropped among pinned tabs becomes one (Zen,
    // removeEssentials with unpin = false).
    if (drag.essential) {
      this.proxy_.handler.setEssential(drag.id, false);
    }
    this.proxy_.handler.moveItem(drag.id, parentId, index);
  }

  // Essentials (zen:drag-and-drop/ZenDragAndDrop.js,
  // #animateVerticalPinnedGridDragOver; zen:tabs/ZenEssentialsPromo.mjs).

  // Zen's canEssentialBeAdded: not a folder, and room left in the grid.
  private canBeEssential_(drag: DragSource): boolean {
    return !drag.folder &&
        (drag.essential || this.view_.essentials.length < MAX_ESSENTIALS);
  }

  protected essentialsPromo_(): boolean {
    return this.essentialsSlot_ >= 0 && this.view_.essentials.length === 0;
  }

  // The grid while a drag is over it: the other essentials with an empty slot
  // where the drag would land (null), and the hidden dragged essential.
  protected essentialCells_(): Array<PinnedRow|null> {
    const essentials = this.view_.essentials;
    if (this.essentialsSlot_ < 0 || !this.drag_) {
      return essentials;
    }
    const dragged = essentials.find(e => e.id === this.drag_!.id);
    const cells: Array<PinnedRow|null> = essentials.filter(e => e !== dragged);
    cells.splice(Math.min(this.essentialsSlot_, cells.length), 0, null);
    return dragged ? [...cells, dragged] : cells;
  }

  protected isDragged_(id: string): boolean {
    return this.essentialsSlot_ >= 0 && this.drag_?.id === id;
  }

  protected onEssentialsDragover_(e: DragEvent) {
    const drag = this.drag_;
    if (!drag || !this.canBeEssential_(drag)) {
      return;
    }
    e.preventDefault();
    if (this.essentialsSlot_ < 0) {
      // The slot opens at the end of the grid (Zen's fake essential tile), or
      // empty Essentials show the promo.
      this.essentialsSlot_ = this.view_.essentials.length;
      if (this.essentialsSlot_ > 0) {
        this.proxy_.handler.playHapticFeedback();
      }
      return;
    }
    if (!this.essentialsPromo_()) {
      this.moveEssentialsSlot_(e, drag);
    }
  }

  // The slot moves to the cell under the dragged tile's centre: an essential
  // keeps the point it was grabbed at, a tab is a tile at the cursor (Zen).
  private moveEssentialsSlot_(e: DragEvent, drag: DragSource) {
    const grid = this.shadowRoot.querySelector<HTMLElement>('#essentials');
    const slot = grid?.querySelector<HTMLElement>('.essential.slot');
    if (!grid || !slot) {
      return;
    }
    const cells = [...grid.querySelectorAll<HTMLElement>(
        '.essential:not(.dragging)')];
    const box = grid.getBoundingClientRect();
    const x = e.clientX - box.left + slot.offsetWidth / 2 -
        (drag.essential ? drag.grabX : 0);
    const y = e.clientY - box.top + slot.offsetHeight / 2 -
        (drag.essential ? drag.grabY : 0);
    const index = cellAt(cells.map(cell => ({
      left: cell.offsetLeft,
      top: cell.offsetTop,
      width: cell.offsetWidth,
      height: cell.offsetHeight,
    })), x, y);
    if (index >= 0 && index !== this.essentialsSlot_) {
      this.essentialsSlot_ = index;
      this.proxy_.handler.playHapticFeedback();
    }
  }

  protected onPromoDragover_() {
    this.promoOver_ = true;
  }

  protected onPromoDragleave_(e: DragEvent) {
    if (!(e.currentTarget as Node).contains(e.relatedTarget as Node | null)) {
      this.promoOver_ = false;
    }
  }

  protected onEssentialsDrop_(e: DragEvent) {
    e.preventDefault();
    const drag = this.drag_;
    const slot = this.essentialsSlot_;
    this.onDragend_();
    if (!drag || !this.canBeEssential_(drag)) {
      return;
    }
    this.addToEssentials_(
        drag.id, drag.kind, slot >= 0 ? slot : this.view_.essentials.length);
  }

  // `id` becomes the essential at `index` of the grid without it.
  private addToEssentials_(id: string, kind: 'item'|'tab', index: number) {
    const others = this.view_.essentials.filter(e => e.id !== id);
    const isEssential = others.length < this.view_.essentials.length;
    if (!isEssential && others.length >= MAX_ESSENTIALS) {
      return;
    }
    const slot = essentialSlot(others, index, this.rootSize_());
    const h = this.proxy_.handler;
    if (kind === 'tab') {
      h.pinTab(id, slot.parentId, slot.index, true);
      return;
    }
    if (!isEssential) {
      h.setEssential(id, true);
    }
    h.moveItem(id, slot.parentId, slot.index);
  }

  // Essentials step aside with Zen's transform transition while a tab is
  // dragged (vertical-tabs.css, #tabbrowser-tabs[movingtab]: transform 0.1s
  // ease-in-out), here from where they were before the render.
  private animateTiles_(before: Map<string, DOMRect>) {
    for (const [id, tile] of this.tiles_()) {
      const from = before.get(id);
      if (!from) {
        continue;
      }
      const to = tile.getBoundingClientRect();
      const dx = from.left - to.left;
      const dy = from.top - to.top;
      if (dx || dy) {
        tile.animate(
            [{transform: `translate(${dx}px, ${dy}px)`}, {transform: 'none'}],
            {duration: 100, easing: 'ease-in-out'});
      }
    }
  }

  private tiles_(): Map<string, HTMLElement> {
    const tiles = this.shadowRoot.querySelectorAll<HTMLElement>(
        '#essentials .essential[data-id]');
    return new Map([...tiles].map(tile => [tile.dataset['id']!, tile]));
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
