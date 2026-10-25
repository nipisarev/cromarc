// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {SnapshotTab, Space} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import type {LibraryElement} from './library.js';
import {getHtml} from './library_spaces.html.js';
import {libraryStrings} from './library_strings.js';
import {buildSidebar} from './sidebar_model.js';
import type {TreeRow} from './sidebar_model.js';
import {iconMask} from './space_icon.js';
import {gradientOf, themeFromJson} from './theme_model.js';
import './folder_icon.js';
import './icon_picker.js';

// What a card lists: its pinned rows (folders, at their depth) and Today.
export interface SpaceCard {
  rows: TreeRow[];
  today: SnapshotTab[];
}

// ZenLibrarySpacesSection: the strip scrolls sideways a little under the
// pointer while a card is dragged to its edge.
const SCROLL_EDGE_PX = 48;
const SCROLL_STEP_PX = 12;

// zen:library/sections/ZenLibrarySpacesSection.mjs: every space as a card
// on its theme, with its tabs, side by side; a card is renamed, given an
// icon or a theme, moved, or opened on one of its tabs. The Library grows
// to hold them. Draws into the Library's shadow root.
export class LibrarySpacesElement extends CrLitElement {
  static get is() {
    return 'cromarc-library-spaces';
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
      cards_: {type: Object},
      renaming_: {type: String},
      picking_: {type: String},
      collapsed_: {type: Object},
      dragging_: {type: String},
      strings: {type: Object},
    };
  }

  accessor library: LibraryElement|null = null;
  accessor spaces: Space[] = [];
  protected accessor cards_: Record<string, SpaceCard> = {};
  protected accessor renaming_: string = '';
  protected accessor picking_: string = '';
  // Folders opened or closed here, without touching the space's own
  // (ZenLibrarySpacesSection #collapsedCopies).
  protected accessor collapsed_: Record<string, boolean> = {};
  protected accessor dragging_: string = '';
  protected accessor strings: typeof libraryStrings = libraryStrings;
  private readonly proxy_ = BrowserProxy.getInstance();
  private drag_: {
    index: number,
    drop: number,
    startX: number,
    centerX: number,
    startScroll: number,
    pointerX: number,
    centers: number[],
    frame: number,
  }|null = null;

  override updated(changed: PropertyValues<this>) {
    super.updated(changed);
    if (changed.has('spaces')) {
      this.load_();
    }
    if (changed.has('renaming_' as keyof LibrarySpacesElement) &&
        this.renaming_) {
      const input =
          this.querySelector<HTMLInputElement>('.space-card-name-input');
      input?.focus();
      input?.select();
    }
    this.requestWidth_();
  }

  // From the Library.

  onShown() {
    this.load_();
    this.requestWidth_();
  }

  onLibraryOpened() {
    this.onShown();
  }

  onEscape(): boolean {
    if (this.picking_ || this.renaming_) {
      this.picking_ = '';
      this.renaming_ = '';
      return true;
    }
    return false;
  }

  private async load_() {
    const cards: Record<string, SpaceCard> = {};
    for (const space of this.spaces) {
      const {snapshot} = await this.proxy_.handler.getSpaceSnapshot(space.id);
      cards[space.id] = snapshot ? {
        rows: buildSidebar(snapshot.tree, []).rows,
        today: snapshot.today,
      } :
                                   {rows: [], today: []};
    }
    this.cards_ = cards;
  }

  // ZenLibrarySpacesSection #updateLibraryWidth: the rail, the cards and the
  // add button.
  private requestWidth_() {
    const list = this.querySelector<HTMLElement>('.spaces');
    const cards = [...this.querySelectorAll<HTMLElement>('.space-card')];
    const plus = this.querySelector<HTMLElement>('.add-space');
    if (!list || this.hidden) {
      return;
    }
    const style = getComputedStyle(list);
    const padding =
        parseFloat(style.paddingLeft) + parseFloat(style.paddingRight);
    const gap = parseFloat(style.columnGap) || 0;
    const first = cards[0];
    const last = cards[cards.length - 1];
    const cardsWidth = first && last ?
        last.offsetLeft + last.offsetWidth - first.offsetLeft + padding :
        padding;
    this.library?.requestWidth(
        110 + cardsWidth + (plus ? plus.offsetWidth : 50) + gap);
  }

  // Template helpers.

  protected cardStyle_(space: Space): string {
    return `--space-gradient: ${gradientOf(themeFromJson(space.theme))}`;
  }

  protected iconMask_(space: Space): string {
    return iconMask(space.icon);
  }

  protected glyph_(space: Space): string {
    return iconMask(space.icon) ? '' : space.icon;
  }

  protected cardOf_(space: Space): SpaceCard {
    return this.cards_[space.id] ?? {rows: [], today: []};
  }

  // A folder closed here hides what is under it.
  protected visibleRows_(space: Space): TreeRow[] {
    const shown: TreeRow[] = [];
    let hiddenBelow = Infinity;
    for (const row of this.cardOf_(space).rows) {
      if (row.depth > hiddenBelow) {
        continue;
      }
      hiddenBelow = Infinity;
      if (row.kind === 'pinned' && row.essential) {
        continue;
      }
      shown.push(row);
      if (row.kind === 'folder' && this.isCollapsed_(row)) {
        hiddenBelow = row.depth;
      }
    }
    return shown;
  }

  protected isCollapsed_(row: TreeRow): boolean {
    return row.kind === 'folder' &&
        (this.collapsed_[row.id] ?? row.collapsed);
  }

  protected rowFavicon_(row: TreeRow): string {
    return row.kind === 'pinned' ? getFaviconForPageURL(row.homeUrl, false) :
                                   '';
  }

  protected todayFavicon_(tab: SnapshotTab): string {
    return tab.favicon ? `url("${tab.favicon}")` :
                         getFaviconForPageURL(tab.url, false);
  }

  protected rowIndent_(row: TreeRow): string {
    return `padding-inline-start: calc(var(--row-padding) + ${
        row.depth} * var(--indent))`;
  }

  // Cards.

  private spaceOf_(e: Event): Space|undefined {
    const id = (e.currentTarget as HTMLElement).dataset['space'];
    return this.spaces.find(space => space.id === id);
  }

  protected onIconClick_(e: Event) {
    const space = this.spaceOf_(e);
    this.picking_ = space && this.picking_ !== space.id ? space.id : '';
  }

  protected onIconPicked_(e: CustomEvent<string>) {
    if (this.picking_) {
      this.proxy_.handler.setSpaceIcon(this.picking_, e.detail);
      this.picking_ = '';
    }
  }

  protected onNameClick_(e: Event) {
    this.renaming_ = this.spaceOf_(e)?.id ?? '';
  }

  protected onNameKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.commitRename_(e);
    } else if (e.key === 'Escape') {
      e.stopPropagation();
      this.renaming_ = '';
    }
  }

  protected onNameBlur_(e: Event) {
    this.commitRename_(e);
  }

  private commitRename_(e: Event) {
    const space = this.spaceOf_(e);
    if (!space || this.renaming_ !== space.id) {
      return;
    }
    const name = (e.target as HTMLInputElement).value.trim();
    this.renaming_ = '';
    if (name && name !== space.name) {
      this.proxy_.handler.renameSpace(space.id, name);
    }
  }

  protected onThemeClick_(e: Event) {
    const space = this.spaceOf_(e);
    if (space) {
      this.fire('library-space-theme', space.id);
    }
  }

  protected onActionsClick_(e: MouseEvent) {
    const space = this.spaceOf_(e);
    if (space) {
      const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
      this.fire('library-space-menu', {id: space.id, x: rect.left, y: rect.bottom});
    }
  }

  protected onAddClick_() {
    this.fire('library-create-space');
  }

  // A row opens the space on it: a folder opens or closes here, a pinned
  // item or a Today tab brings its space's window up with it.
  protected onRowClick_(e: Event) {
    const {space, row, url} = (e.currentTarget as HTMLElement).dataset;
    if (!space) {
      return;
    }
    const folder = this.cards_[space]?.rows.find(
        r => r.id === row && r.kind === 'folder');
    if (folder) {
      this.collapsed_ = {...this.collapsed_, [folder.id]: !this.isCollapsed_(folder)};
      return;
    }
    const current = this.spaces.find(s => s.id === space)?.current;
    // The window this Library is in may go: it shuts at once.
    this.fire(current ? 'library-close' : 'library-close-now');
    this.proxy_.handler.openSpaceEntry(space, row ?? '', url ?? '');
  }

  // Moving a card by its handle (ZenLibrarySpacesSection #onPointerDown and
  // on): it follows the pointer, the others step aside, and the order is the
  // spaces' order once it is let go.

  protected onHandlePointerdown_(e: PointerEvent) {
    if (e.button !== 0) {
      return;
    }
    const cards = [...this.querySelectorAll<HTMLElement>('.space-card')];
    const card = (e.currentTarget as HTMLElement).closest<HTMLElement>(
        '.space-card');
    const list = this.querySelector<HTMLElement>('.spaces');
    if (!card || !list) {
      return;
    }
    const index = cards.indexOf(card);
    const rect = card.getBoundingClientRect();
    this.drag_ = {
      index,
      drop: index,
      startX: e.clientX,
      centerX: rect.left + rect.width / 2,
      startScroll: list.scrollLeft,
      pointerX: e.clientX,
      centers: cards.filter((_, i) => i !== index).map(other => {
        const r = other.getBoundingClientRect();
        return r.left + r.width / 2;
      }),
      frame: 0,
    };
    this.dragging_ = card.dataset['space'] ?? '';
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    e.preventDefault();
    this.autoScroll_();
  }

  protected onHandlePointermove_(e: PointerEvent) {
    if (this.drag_) {
      this.drag_.pointerX = e.clientX;
      this.updateDrag_();
    }
  }

  private updateDrag_() {
    const drag = this.drag_;
    const list = this.querySelector<HTMLElement>('.spaces');
    if (!drag || !list) {
      return;
    }
    const cards = [...this.querySelectorAll<HTMLElement>('.space-card')];
    const travel =
        drag.pointerX - drag.startX + list.scrollLeft - drag.startScroll;
    cards[drag.index]!.style.translate = `${travel}px 0`;
    const center = drag.centerX + travel;
    const drop = drag.centers.filter(c => center > c).length;
    if (drop !== drag.drop) {
      drag.drop = drop;
      cards.forEach((card, i) => {
        let shift = '';
        if (drag.index < i && i <= drop) {
          shift = 'left';
        } else if (drop <= i && i < drag.index) {
          shift = 'right';
        }
        shift ? card.setAttribute('shift', shift) : card.removeAttribute('shift');
      });
    }
  }

  private autoScroll_() {
    const drag = this.drag_;
    const list = this.querySelector<HTMLElement>('.spaces');
    if (!drag || !list) {
      return;
    }
    const rect = list.getBoundingClientRect();
    let step = 0;
    if (drag.pointerX < rect.left + SCROLL_EDGE_PX) {
      step = -SCROLL_STEP_PX;
    } else if (drag.pointerX > rect.right - SCROLL_EDGE_PX) {
      step = SCROLL_STEP_PX;
    }
    if (step) {
      const before = list.scrollLeft;
      list.scrollLeft += step;
      if (list.scrollLeft !== before) {
        this.updateDrag_();
      }
    }
    drag.frame = requestAnimationFrame(() => this.autoScroll_());
  }

  protected onHandlePointercancel_(e: PointerEvent) {
    this.onHandlePointerup_(e);
  }

  protected async onHandlePointerup_(e: PointerEvent) {
    const drag = this.drag_;
    if (!drag) {
      return;
    }
    (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);
    cancelAnimationFrame(drag.frame);
    this.drag_ = null;
    const list = this.querySelector<HTMLElement>('.spaces');
    list?.setAttribute('no-transition', '');
    if (drag.drop !== drag.index && e.type === 'pointerup') {
      // Zen's reorderWorkspace(uuid, dropIndex): among the others, at the
      // slot it was let go over.
      const ids = this.spaces.map(space => space.id);
      const order = ids.filter((_, i) => i !== drag.index);
      order.splice(drag.drop, 0, ids[drag.index]!);
      this.proxy_.handler.reorderSpaces(order);
    }
    for (const card of this.querySelectorAll<HTMLElement>('.space-card')) {
      card.removeAttribute('shift');
      card.style.translate = '';
    }
    this.dragging_ = '';
    await new Promise(resolve => requestAnimationFrame(resolve));
    list?.removeAttribute('no-transition');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-library-spaces': LibrarySpacesElement;
  }
}

customElements.define(LibrarySpacesElement.is, LibrarySpacesElement);
