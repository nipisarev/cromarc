// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {physicalSpring, THEME_DOT_SPRING} from '../sidebar/motion.js';
import {calculateCompliments, clampToPad, colorFromPosition, DEFAULT_THEME, harmoniesFor, initialPosition, LINE_PATH, MAX_COLORS, opacityProgress, textureAt, themeFromJson, wavePath} from '../sidebar/theme_model.js';
import type {DotType, PickerDot, Point, Rgb, Scheme, SpaceTheme} from '../sidebar/theme_model.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '../theme_picker.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {Preset} from './presets.js';
import {PRESET_PAGES} from './presets.js';

// zen:spaces/zen-gradient-generator.css: the grain dial is 6rem on macOS, at
// Firefox's 13px menu font (app.css, #texture).
const DIAL = 78;

interface Dot extends PickerDot {
  color: Rgb;
}

// Zen's theme picker (zen:spaces/ZenGradientGenerator.mjs, nsZenThemePicker;
// zen-panels/theme-picker.inc): the pad, where the primary dot picks the
// colour and the others follow it in a harmony; the preset pages; opacity
// and grain. Every change is previewed under the window; the browser saves
// the theme when the panel closes.
export class ThemePickerAppElement extends CrLitElement {
  static get is() {
    return 'cromarc-theme-picker-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme_: {type: Object},
      dots_: {type: Array},
      shown_: {type: Object},
      page_: {type: Number},
      dragging_: {type: Boolean},
    };
  }

  protected accessor theme_: SpaceTheme = {...DEFAULT_THEME};
  // Sorted by id, the primary one first.
  protected accessor dots_: Dot[] = [];
  // Where each dot is drawn while its spring runs, by id.
  protected accessor shown_: Record<number, Point> = {};
  protected accessor page_: number = 0;
  protected accessor dragging_: boolean = false;
  protected readonly pages_ = PRESET_PAGES;

  private readonly handler_ = new PageHandlerRemote();
  private readonly router_ = new PageCallbackRouter();
  // Zen's useAlgo and #currentLightness.
  private harmony_ = '';
  private lightness_ = 50;
  private recentlyDragged_ = false;
  private lastHaptic_ = -1;
  private springs_ = new Map<number, number>();
  private resizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.router_.open.addListener(
        (_spaceId: string, theme: string) => this.onOpen_(theme));
    PageHandlerFactory.getRemote().createPageHandler(
        this.router_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
    document.addEventListener('keydown', e => {
      if (e.key === 'Escape') {
        this.handler_.close();
      }
    });
  }

  override firstUpdated() {
    const panel = this.shadowRoot.querySelector('#panel')!;
    this.resizeObserver_ = new ResizeObserver(() => {
      this.handler_.setHeight(Math.ceil(panel.getBoundingClientRect().height));
    });
    this.resizeObserver_.observe(panel);
  }

  private onOpen_(json: string) {
    this.theme_ = json ? themeFromJson(json) : {...DEFAULT_THEME};
    this.harmony_ = this.theme_.harmony;
    this.lightness_ = this.theme_.lightness;
    this.dots_ = this.theme_.colors.map((color, id) => {
      const dot = this.theme_.dots[id];
      return {
        id,
        color,
        position: dot ? {x: dot.x, y: dot.y} : initialPosition(color),
        type: dot?.type ?? 'float',
      };
    });
    this.stopSprings_();
    this.shown_ = {};
    this.lastHaptic_ = Math.round(this.theme_.opacity * 10);
  }

  protected shownAt_(dot: Dot): Point {
    return this.shown_[dot.id] ?? dot.position;
  }

  protected isDefault_(): boolean {
    return this.dots_.length === 0;
  }

  protected canAdd_(): boolean {
    return this.dots_.length > 0 && this.dots_.length < MAX_COLORS;
  }

  protected canToggle_(): boolean {
    return this.dots_.length >= 2;
  }

  protected progress_(): number {
    return opacityProgress(this.theme_.opacity, this.isDefault_());
  }

  protected wavePath_(): string {
    return wavePath(this.progress_());
  }

  protected waveIsLine_(): boolean {
    return this.wavePath_() === LINE_PATH;
  }

  // How far the opacity is along its range: the wave and the thumb grow.
  protected thumbStyle_(): string {
    const progress = this.progress_();
    return `--thumb-height: ${40 + progress * 15}px; ` +
        `--thumb-width: ${10 + progress * 15}px`;
  }

  // initTextureInput: 16 dots round the dial, the first at 3 o'clock.
  protected textureDotStyle_(index: number): string {
    const angle = index / 16 * Math.PI * 2;
    return `left: ${Math.cos(angle) * 50 + 50}%; top: ${Math.sin(angle) * 50 + 50}%`;
  }

  // The dial's handle, round its edge.
  protected textureHandleStyle_(): string {
    const width = DIAL;
    const rotation = this.theme_.texture * 360 - 90;
    const radians = rotation * Math.PI / 180;
    const top = Math.sin(radians) * (width / 2) - 6 + width / 2;
    const left = Math.cos(radians) * (width / 2) - 3 + width / 2;
    return `transform: rotate(${rotation + 90}deg); top: ${top}px; left: ${left}px`;
  }

  // The pad.

  protected onPadClick_(e: MouseEvent) {
    if (this.dragging_ || this.recentlyDragged_ || e.button !== 0) {
      return;
    }
    const target = e.composedPath()[0] as HTMLElement;
    if (target.closest('button') || target.classList.contains('dot')) {
      return;
    }
    const point = clampToPad(this.padPoint_(e));
    const primary = this.dots_.find(d => d.id === 0);
    if (!primary) {
      // spawnDot: the first dot, placed by hand.
      this.applyPositions_([{id: 0, position: point, type: 'float'}]);
    } else {
      this.applyPositions_(this.compliments_(
          [{...primary, position: point}, ...this.dots_.filter(d => d.id !== 0)],
          'update', this.harmony_));
    }
    this.preview_();
  }

  protected onDotPointerdown_(e: PointerEvent) {
    if (e.button !== 0) {
      return;
    }
    e.preventDefault();
    this.dragging_ = true;
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  }

  protected onDotPointermove_(e: PointerEvent) {
    if (!this.dragging_) {
      return;
    }
    const point = clampToPad(this.padPoint_(e));
    this.applyPositions_(this.compliments_(
        this.dots_.map(d => d.id === 0 ? {...d, position: point} : d),
        'update', this.harmony_));
    this.preview_();
  }

  protected onDotPointerup_() {
    if (!this.dragging_) {
      return;
    }
    this.dragging_ = false;
    this.recentlyDragged_ = true;
    setTimeout(() => this.recentlyDragged_ = false, 100);
  }

  // onDotMouseUp with the right button: the dot goes, the others close up.
  protected onDotContextmenu_(e: MouseEvent) {
    e.preventDefault();
    this.removeDot_(0);
  }

  protected onAddClick_() {
    if (!this.canAdd_()) {
      return;
    }
    this.applyPositions_(this.compliments_(this.dots_, 'add', this.harmony_));
    this.preview_();
  }

  protected onRemoveClick_() {
    const last = this.dots_.at(-1);
    if (last) {
      this.removeDot_(last.id);
    }
  }

  protected onToggleClick_() {
    const options = harmoniesFor(this.dots_.length);
    if (!options.length) {
      return;
    }
    const index = options.indexOf(this.harmony_);
    const next = options[index === -1 ? 0 : (index + 1) % options.length]!;
    this.applyPositions_(this.compliments_(this.dots_, 'update', next));
    this.preview_();
  }

  protected onSchemeClick_(e: Event) {
    const scheme = (e.currentTarget as HTMLElement).dataset['scheme'] as Scheme;
    this.theme_ = {...this.theme_, scheme};
    this.handler_.setScheme(scheme);
    this.preview_();
  }

  // The presets.

  protected onPresetClick_(e: Event) {
    const [page, index] = ((e.currentTarget as HTMLElement).dataset['preset'] ??
                           '0,0').split(',').map(Number);
    const preset: Preset = this.pages_[page!]![index!]!;
    this.dots_ = this.dots_.slice(0, preset.numDots);
    const type: DotType = preset.type ?? 'explicit-lightness';
    const [x, y] = preset.position;
    const dots: PickerDot[] = [{id: 0, position: {x, y}, type}];
    for (let id = 1; id < preset.numDots; id++) {
      dots.push({id, position: {x: 0, y: 0}, type});
    }
    if (preset.lightness !== undefined) {
      this.lightness_ = preset.lightness;
    }
    this.applyPositions_(this.compliments_(dots, 'update', preset.algo));
    this.preview_();
  }

  protected onPageLeftClick_() {
    this.scrollToPage_((this.page_ - 1 + this.pages_.length) % this.pages_.length);
  }

  protected onPageRightClick_() {
    this.scrollToPage_((this.page_ + 1) % this.pages_.length);
  }

  protected onPagesWheel_(e: WheelEvent) {
    e.preventDefault();
    e.stopPropagation();
  }

  private scrollToPage_(page: number) {
    this.page_ = page;
    const pages = this.shadowRoot.querySelector<HTMLElement>('#pages')!;
    pages.scrollLeft = page * pages.scrollWidth / this.pages_.length;
  }

  // Opacity and grain.

  protected onOpacityInput_(e: Event) {
    const opacity = Number((e.target as HTMLInputElement).value);
    // A tick at every tenth, as Zen's onOpacityChange.
    if (Math.round(opacity * 10) !== this.lastHaptic_) {
      this.handler_.playHapticFeedback();
      this.lastHaptic_ = Math.round(opacity * 10);
    }
    this.theme_ = {...this.theme_, opacity};
    this.preview_();
  }

  protected onTexturePointerdown_(e: PointerEvent) {
    e.preventDefault();
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  }

  protected onTexturePointermove_(e: PointerEvent) {
    const handle = e.currentTarget as HTMLElement;
    if (!handle.hasPointerCapture(e.pointerId)) {
      return;
    }
    const dial = handle.parentElement!.getBoundingClientRect();
    const texture = textureAt(e.clientX - dial.left - dial.width / 2,
                              e.clientY - dial.top - dial.height / 2);
    if (texture !== this.theme_.texture) {
      this.theme_ = {...this.theme_, texture};
      this.handler_.playHapticFeedback();
      this.preview_();
    }
  }

  // Zen's calculateCompliments on this picker; keeps the harmony it settles
  // on.
  private compliments_(
      dots: PickerDot[], action: 'add'|'remove'|'update',
      harmony: string): PickerDot[] {
    const result =
        calculateCompliments(dots, this.dots_.length, action, harmony);
    if (result.harmony) {
      this.harmony_ = result.harmony;
    }
    return result.dots;
  }

  // Zen's handleColorPositions: the dots move to `positions`, taking the
  // colour of where they land; missing ones appear. They spring there unless
  // the primary one is being dragged.
  private applyPositions_(positions: PickerDot[]) {
    const dots = [...this.dots_];
    for (const target of [...positions].sort((a, b) => a.id - b.id)) {
      const {color, lightness} =
          colorFromPosition(target.position, target.type, this.lightness_);
      this.lightness_ = lightness;
      const index = dots.findIndex(d => d.id === target.id);
      if (index === -1) {
        dots.push({...target, color});
        continue;
      }
      if (!this.dragging_) {
        this.spring_(target.id, this.shownAt_(dots[index]!), target.position);
      }
      dots[index] = {...target, color};
    }
    this.dots_ = dots.sort((a, b) => a.id - b.id);
  }

  private removeDot_(id: number) {
    const remaining = this.dots_.filter(d => d.id !== id)
                          .map((d, index) => ({...d, id: index}));
    this.dots_ = remaining;
    this.applyPositions_(this.compliments_(remaining, 'remove', ''));
    this.preview_();
  }

  private spring_(id: number, from: Point, to: Point) {
    if (from.x === to.x && from.y === to.y) {
      return;
    }
    const x = physicalSpring(from.x, to.x, THEME_DOT_SPRING);
    const y = physicalSpring(from.y, to.y, THEME_DOT_SPRING);
    const start = performance.now();
    cancelAnimationFrame(this.springs_.get(id) ?? 0);
    const step = (now: number) => {
      const t = (now - start) / 1000;
      const sx = x(t);
      const sy = y(t);
      if (sx.done && sy.done) {
        const shown = {...this.shown_};
        delete shown[id];
        this.shown_ = shown;
        this.springs_.delete(id);
        return;
      }
      this.shown_ = {...this.shown_, [id]: {x: sx.value, y: sy.value}};
      this.springs_.set(id, requestAnimationFrame(step));
    };
    this.springs_.set(id, requestAnimationFrame(step));
  }

  private stopSprings_() {
    for (const frame of this.springs_.values()) {
      cancelAnimationFrame(frame);
    }
    this.springs_.clear();
  }

  private padPoint_(e: MouseEvent): Point {
    const pad = this.shadowRoot.querySelector('#pad')!.getBoundingClientRect();
    return {x: e.clientX - pad.left, y: e.clientY - pad.top};
  }

  // Zen's updateCurrentWorkspace without saving: the window shows it.
  private preview_() {
    this.theme_ = {
      ...this.theme_,
      colors: this.dots_.map(d => d.color),
      dots: this.dots_.map(d => ({...d.position, type: d.type})),
      lightness: this.lightness_,
      harmony: this.harmony_,
    };
    this.handler_.preview(JSON.stringify(this.theme_));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-theme-picker-app': ThemePickerAppElement;
  }
}

customElements.define(ThemePickerAppElement.is, ThemePickerAppElement);
