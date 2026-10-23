// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SidebarAppElement} from './app.js';

export function getHtml(this: SidebarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="sidebar" @contextmenu="${this.onBackgroundContextmenu_}">
  <div id="essentials" ?empty="${this.view_.essentials.length === 0}"
      @dragover="${this.onEssentialsDragover_}"
      @drop="${this.onEssentialsDrop_}">
    ${this.view_.essentials.map(e => html`
      <div class="essential" data-id="${e.id}" data-kind="item"
          draggable="true" title="${e.title}" state="${e.state}"
          ?active="${this.isActive_(e.tab)}"
          @click="${this.onItemClick_}"
          @contextmenu="${this.onItemContextmenu_}"
          @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}">
        <span class="favicon"
            style="background-image: ${this.faviconFor_(e.homeUrl, e.tab)}">
        </span>
      </div>`)}
  </div>
  <div id="tree" role="tree">
    ${this.view_.rows.map(row => row.kind === 'folder' ? html`
      <div class="row folder ${this.dropClass_(row.id)}" data-id="${row.id}"
          data-kind="item" draggable="true" style="--depth: ${row.depth}"
          ?collapsed="${row.collapsed}"
          @click="${this.onFolderClick_}"
          @contextmenu="${this.onItemContextmenu_}"
          @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}"
          @dragover="${this.onRowDragover_}" @drop="${this.onRowDrop_}">
        <span class="chevron"></span>
        <span class="folder-icon"></span>
        ${this.renaming_ === row.id ? html`
          <input class="rename" .value="${row.title}"
              @keydown="${this.onRenameKeydown_}"
              @blur="${this.onRenameBlur_}" @click="${this.onRenameClick_}">` :
          html`<span class="title" @dblclick="${this.onTitleDblclick_}">${
              row.title}</span>`}
      </div>` : html`
      <div class="row pinned ${this.dropClass_(row.id)}" data-id="${row.id}"
          data-kind="item" draggable="true" style="--depth: ${row.depth}"
          state="${row.state}" ?active="${this.isActive_(row.tab)}"
          title="${this.pinnedTooltip_(row)}"
          @click="${this.onItemClick_}"
          @contextmenu="${this.onItemContextmenu_}"
          @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}"
          @dragover="${this.onRowDragover_}" @drop="${this.onRowDrop_}">
        <span class="favicon"
            style="background-image: ${this.faviconFor_(row.homeUrl, row.tab)}">
        </span>
        ${this.renaming_ === row.id ? html`
          <input class="rename" .value="${row.title}"
              @keydown="${this.onRenameKeydown_}"
              @blur="${this.onRenameBlur_}" @click="${this.onRenameClick_}">` :
          html`<span class="title" @dblclick="${this.onTitleDblclick_}">${
              row.title}</span>`}
        ${row.state === 'away' ? html`
          <button class="reset" title="${this.strings.backToHome}"
              @click="${this.onResetClick_}">↩</button>` : nothing}
        ${row.tab ? html`
          <button class="close" title="${this.strings.close}"
              data-tab="${row.tab.id}" @click="${this.onCloseTabClick_}">×
          </button>` : nothing}
      </div>`)}
    <div id="tree-end" @dragover="${this.onTreeEndDragover_}"
        @drop="${this.onTreeEndDrop_}"></div>
  </div>
  <div class="separator"></div>
  <div class="row new-tab" @click="${this.onNewTabClick_}">
    <span class="plus">＋</span>
    <span class="title">${this.strings.newTab}</span>
  </div>
  <div id="today" @dragover="${this.onTodayDragover_}"
      @drop="${this.onTodayDrop_}">
    ${this.view_.today.map(tab => html`
      <div class="row today" data-id="${tab.id}" data-kind="tab"
          draggable="true" ?active="${tab.isActive}" title="${tab.title}"
          @click="${this.onTodayClick_}"
          @contextmenu="${this.onTodayContextmenu_}"
          @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}">
        <span class="favicon"
            style="background-image: ${this.faviconFor_(tab.url, tab)}">
        </span>
        <span class="title">${tab.title || tab.url}</span>
        <button class="close" title="${this.strings.close}"
            data-tab="${tab.id}" @click="${this.onCloseTabClick_}">×</button>
      </div>`)}
  </div>
</div>
${this.menu_ ? html`
  <div id="menu-backdrop" @click="${this.onMenuBackdropClick_}"
      @contextmenu="${this.onMenuBackdropContextmenu_}"></div>
  <div id="menu" role="menu"
      style="left: ${this.menu_.x}px; top: ${this.menu_.y}px">
    ${this.menu_.items.map((item, index) => html`
      <div class="menu-item" role="menuitem" data-index="${index}"
          @click="${this.onMenuItemClick_}">${item.label}</div>`)}
  </div>` : nothing}
<!--_html_template_end_-->`;
  // clang-format on
}
