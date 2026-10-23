// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {TabListModel} from './tab_list_model.js';

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
    };
  }

  protected accessor tabs_: Tab[] = [];

  private readonly service_: TabStripServiceRemote =
      TabStripService.getRemote();
  // Bumped on every snapshot; observers of older snapshots go quiet.
  private generation_ = 0;

  override connectedCallback() {
    super.connectedCallback();
    this.connect_();
  }

  // Reads the tab strip and follows its events. Called again whenever an
  // event does not fit the model, so the sidebar never drifts from the window.
  private async connect_() {
    const generation = ++this.generation_;
    const snapshot = await this.service_.getTabs();
    if (generation !== this.generation_) {
      return;
    }
    const model = new TabListModel(snapshot.tabStrip);
    const apply = (ok: boolean) => {
      if (generation !== this.generation_) {
        return;
      }
      if (!ok) {
        this.connect_();
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

  protected onTabClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['id'];
    if (id) {
      // The tab may close before the call lands; the next event resyncs.
      this.service_.activateTab(id).catch(() => {});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cromarc-sidebar-app': SidebarAppElement;
  }
}

customElements.define(SidebarAppElement.is, SidebarAppElement);
