// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './theme_editor.css.js';
import {getHtml} from './theme_editor.html.js';
import {colorAt, DEFAULT_THEME, HARMONIES, harmonyPoints, MAX_COLORS, MAX_OPACITY, MIN_OPACITY, pointOf} from './theme_model.js';
import type {Point, Scheme, SpaceTheme} from './theme_model.js';

// The colour wheel's size in px.
const WHEEL = 200;

// Zen's theme picker (zen:spaces/ZenGradientGenerator.mjs): up to three dots
// on a colour wheel, the others following the first in a harmony; opacity,
// grain and light or dark. Fires `theme-change` with each change (for the
// preview) and `theme-done` with the theme to keep.
export class ThemeEditorElement extends CrLitElement {
  static get is() {
    return 'cromarc-theme-editor';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme: {type: Object},
      points_: {type: Array},
      harmony_: {type: String},
      minOpacity_: {type: Number},
      maxOpacity_: {type: Number},
    };
  }

  accessor theme: SpaceTheme = {...DEFAULT_THEME};
  protected accessor points_: Point[] = [];
  protected accessor harmony_: string = '';
  protected accessor minOpacity_: number = MIN_OPACITY;
  protected accessor maxOpacity_: number = MAX_OPACITY;
  private dragging_ = -1;

  override connectedCallback() {
    super.connectedCallback();
    this.points_ = this.theme.colors.map(c => pointOf(c, WHEEL));
    // The saved harmony, so the toggle goes on from it.
    this.harmony_ = this.theme.harmony;
  }

  protected dotStyle_(point: Point, index: number): string {
    const [r, g, b] = this.theme.colors[index] ?? [0, 0, 0];
    return `left: ${point.x}px; top: ${point.y}px; background: rgb(${r}, ${g}, ${b})`;
  }

  // Zen's names (zen-panel-ui-gradient-generator-algo-*); free dots are
  // "floating".
  protected harmonyLabel_(): string {
    const labels: Record<string, string> = {
      complementary: 'Комплементарное',
      singleAnalogous: 'Аналоговое',
      splitComplementary: 'Раздельное',
      analogous: 'Аналоговое',
      triadic: 'Триада',
    };
    return labels[this.harmony_] ?? 'Плавающее';
  }

  protected onWheelPointerdown_(e: PointerEvent) {
    const target = e.target as HTMLElement;
    if (target.classList.contains('dot')) {
      this.dragging_ = Number(target.dataset['index']);
      (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
      return;
    }
    if (this.points_.length >= MAX_COLORS) {
      return;
    }
    const wheel = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const point = {x: e.clientX - wheel.left, y: e.clientY - wheel.top};
    const first = this.points_[0] ?? point;
    this.setPoints_(harmonyPoints(first, this.points_.length + 1, WHEEL));
  }

  protected onWheelPointermove_(e: PointerEvent) {
    if (this.dragging_ < 0) {
      return;
    }
    const wheel = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const point = {
      x: Math.min(WHEEL, Math.max(0, e.clientX - wheel.left)),
      y: Math.min(WHEEL, Math.max(0, e.clientY - wheel.top)),
    };
    // Moving the first dot moves the harmony; the others move alone.
    if (this.dragging_ === 0) {
      this.setPoints_(
          harmonyPoints(point, this.points_.length, WHEEL, this.harmony_));
    } else {
      const points = [...this.points_];
      points[this.dragging_] = point;
      this.setPoints_(points, '');
    }
  }

  protected onWheelPointerup_() {
    this.dragging_ = -1;
  }

  protected onDotDblclick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const points = this.points_.filter((_, i) => i !== index);
    this.setPoints_(points.length ?
                        harmonyPoints(points[0]!, points.length, WHEEL) :
                        []);
  }

  protected onHarmonyClick_() {
    const options =
        HARMONIES.filter(h => h.angles.length + 1 === this.points_.length);
    if (!options.length) {
      return;
    }
    const index = options.findIndex(h => h.type === this.harmony_);
    const next = options[(index + 1) % options.length]!;
    this.setPoints_(
        harmonyPoints(this.points_[0]!, this.points_.length, WHEEL, next.type),
        next.type);
  }

  protected onOpacityInput_(e: Event) {
    this.change_({opacity: Number((e.target as HTMLInputElement).value)});
  }

  protected onTextureInput_(e: Event) {
    this.change_({texture: Number((e.target as HTMLInputElement).value)});
  }

  protected onSchemeClick_(e: Event) {
    this.change_({
      scheme: (e.currentTarget as HTMLElement).dataset['scheme'] as Scheme,
    });
  }

  protected onDoneClick_() {
    this.fire('theme-done', this.theme);
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' || e.key === 'Enter') {
      this.onDoneClick_();
    }
  }

  private setPoints_(points: Point[], harmony?: string) {
    this.points_ = points;
    this.harmony_ = harmony ??
        HARMONIES.find(h => h.angles.length + 1 === points.length)?.type ?? '';
    this.change_({
      colors: points.map(p => colorAt(p, WHEEL)),
      harmony: this.harmony_,
    });
  }

  private change_(change: Partial<SpaceTheme>) {
    this.theme = {...this.theme, ...change};
    this.fire('theme-change', this.theme);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-theme-editor': ThemeEditorElement;
  }
}

customElements.define(ThemeEditorElement.is, ThemeEditorElement);
