// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SidebarAppElement} from './app.js';

export function getHtml(this: SidebarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="viewport" style="background: ${this.tint_}">
  ${this.stripSpaces_.map((space, index) => html`
  <div class="strip" style="${this.stripStyle_(index)}">
  ${space.current ? html`
    <div id="sidebar" class="sidebar"
          @contextmenu="${this.onBackgroundContextmenu_}">
      <div id="essentials" class="essentials"
          ?empty="${this.view_.essentials.length === 0}"
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
      <div class="space-header ${this.dropZone_ === 'header' ? 'over' : ''}"
          @dragover="${this.onHeaderDragover_}"
          @dragleave="${this.onZoneDragleave_}"
          @drop="${this.onHeaderDrop_}">
        <span class="space-icon">
          <span class="svg" style="mask-image: ${this.iconMask_(space.icon)}"
              ?hidden="${!this.iconMask_(space.icon)}"></span>
          ${this.iconText_(space.icon)}
        </span>
        <span class="space-name" @dblclick="${this.onSpaceNameDblclick_}">${
            space.name}</span>
        <button class="space-menu-button" title="${this.strings.editSpace}"
            @click="${this.onSpaceMenuClick_}">⋯</button>
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
            ${row.state === 'away' ? html`
              <button class="reset-pin" title="${this.strings.resetPinButton}"
                  @click="${this.onResetClick_}"
                  @mousemove="${this.onResetMousemove_}">
                <span class="favicon"
                    style="background-image: ${this.faviconFor_(row.homeUrl, null)}">
                </span>
              </button>` : nothing}
            <span class="favicon current"
                style="background-image: ${this.faviconFor_(row.homeUrl, row.tab)}">
            </span>
            ${this.renaming_ === row.id ? html`
              <input class="rename" .value="${row.title}"
                  @keydown="${this.onRenameKeydown_}"
                  @blur="${this.onRenameBlur_}" @click="${this.onRenameClick_}">` :
              html`<span class="label">
                <span class="title" @dblclick="${this.onTitleDblclick_}">${
                    row.title}</span>
                ${row.state === 'away' ? html`<span class="sublabel">${
                    this.resetAccel_ ? this.strings.separateFromPinned :
                                       this.strings.backToPinned}</span>` :
                    nothing}
              </span>`}
            ${row.tab ? html`
              <button class="close" title="${this.strings.close}"
                  data-tab="${row.tab.id}" @click="${this.onCloseTabClick_}">×
              </button>` : nothing}
          </div>`)}
        <div id="tree-end" class="${this.dropZone_ === 'end' ? 'over' : ''}"
            @dragover="${this.onTreeEndDragover_}"
            @dragleave="${this.onZoneDragleave_}"
            @drop="${this.onTreeEndDrop_}"></div>
      </div>
      <div class="separator" @dragover="${this.onTreeEndDragover_}"
          @dragleave="${this.onZoneDragleave_}"
          @drop="${this.onTreeEndDrop_}"></div>
      ${this.view_.attention.length ? html`
        <div class="section-header attention">⚠ ${this.strings.needsAttention}</div>
        ${this.view_.attention.map(tab => html`
          <div class="row attention" data-id="${tab.id}" title="${tab.title}"
              ?active="${tab.isActive}" @click="${this.onTodayClick_}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(tab.url, tab)}">
            </span>
            <span class="title">${tab.title || tab.url}
              <span class="subtitle">${this.strings.unsavedInput}</span>
            </span>
          </div>
          <div class="attention-actions" data-id="${tab.id}">
            <button class="text" data-tab="${tab.id}"
                @click="${this.onCloseTabClick_}">${this.strings.closeAnyway}
            </button>
            <button class="text" @click="${this.onKeepClick_}">${
                this.strings.keep}</button>
          </div>`)}` : nothing}
      <div class="section-header" @dragover="${this.onTreeEndDragover_}"
          @dragleave="${this.onZoneDragleave_}"
          @drop="${this.onTreeEndDrop_}">
        <span>${this.strings.today}</span>
        <button class="text" ?hidden="${this.view_.today.length === 0}"
            @click="${this.onClearClick_}">${this.strings.clear}</button>
      </div>
      <div class="row new-tab" @click="${this.onNewTabClick_}">
        <span class="plus">＋</span>
        <span class="title">${this.strings.newTab}</span>
      </div>
      <div id="today" @dragover="${this.onTodayDragover_}"
          @drop="${this.onTodayDrop_}">
        ${this.view_.today.map(tab => html`
          <div class="row today" data-id="${tab.id}" data-kind="tab"
              draggable="true" ?active="${tab.isActive}"
              title="${this.todayTooltip_(tab)}"
              @click="${this.onTodayClick_}"
              @contextmenu="${this.onTodayContextmenu_}"
              @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(tab.url, tab)}">
            </span>
            <span class="title">${tab.title || tab.url}</span>
            <button class="close" title="${this.strings.close}"
                data-tab="${tab.id}" @click="${this.onCloseTabClick_}">×</button>
            <span class="countdown" style="width: ${this.countdown_(tab)}"></span>
          </div>`)}
      </div>
    </div>` : html`
    <div class="sidebar snapshot">
      <div class="essentials"
          ?empty="${this.snapshotOf_(space.id).view.essentials.length === 0}">
        ${this.snapshotOf_(space.id).view.essentials.map(e => html`
          <div class="essential" state="asleep" title="${e.title}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(e.homeUrl, null)}">
            </span>
          </div>`)}
      </div>
      <div class="space-header">
        <span class="space-icon">
          <span class="svg" style="mask-image: ${this.iconMask_(space.icon)}"
              ?hidden="${!this.iconMask_(space.icon)}"></span>
          ${this.iconText_(space.icon)}
        </span>
        <span class="space-name">${space.name}</span>
      </div>
      ${this.snapshotOf_(space.id).view.rows.map(row => row.kind === 'folder' ?
        html`
          <div class="row folder" style="--depth: ${row.depth}"
              ?collapsed="${row.collapsed}">
            <span class="chevron"></span>
            <span class="folder-icon"></span>
            <span class="title">${row.title}</span>
          </div>` : html`
          <div class="row pinned" state="asleep" style="--depth: ${row.depth}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(row.homeUrl, null)}">
            </span>
            <span class="title">${row.title}</span>
          </div>`)}
      <div class="separator"></div>
      <div class="section-header"><span>${this.strings.today}</span></div>
      ${this.snapshotOf_(space.id).today.map(t => html`
        <div class="row today">
          <span class="favicon"
              style="background-image: ${this.snapshotFavicon_(t.url, t.favicon)}">
          </span>
          <span class="title">${t.title || t.url}</span>
        </div>`)}
    </div>`}
  </div>`)}
</div>
${this.editor_ ? html`
  <cromarc-space-editor id="space-editor" ?creating="${!this.editor_.id}"
      .name="${this.editor_.name}" .icon="${this.editor_.icon}"
      .pickerOpen="${this.editor_.pickerOpen}"
      .remoteSpaces="${this.remoteSpaces_}"
      @space-connect="${this.onSpaceConnect_}"
      @space-edit-done="${this.onSpaceEditDone_}"
      @space-edit-cancel="${this.onSpaceEditCancel_}">
  </cromarc-space-editor>` : nothing}
${this.themeEditor_ ? html`
  <cromarc-theme-editor id="theme-editor" .theme="${this.themeEditor_.theme}"
      @theme-change="${this.onThemeChange_}"
      @theme-done="${this.onThemeDone_}">
  </cromarc-theme-editor>` : nothing}
${this.icloud_ ? html`
  <div id="icloud" class="card" role="dialog">
    <div class="title">${this.strings.icloudTitle} · ${this.icloud_.space.name}</div>
    <p>${this.strings.icloudBody}</p>
    <p class="state">${this.icloudState_()}</p>
    <div class="buttons">
      ${this.icloud_.enabled !== this.icloud_.active ? html`
        <button class="primary" @click="${this.onRestartClick_}">${
            this.strings.restart}</button>` : nothing}
      <button id="icloud-toggle" @click="${this.onICloudToggleClick_}">${
          this.icloud_.enabled ? this.strings.icloudDisable :
                                 this.strings.icloudEnable}</button>
      <button @click="${this.onICloudCloseClick_}">${this.strings.close}</button>
    </div>
  </div>` : nothing}
${this.deleting_ ? html`
  <div id="delete-space" role="alertdialog">
    <div class="title">${this.strings.deleteSpaceTitle}</div>
    <p>${this.strings.deleteSpaceBody(this.deleting_.name)}</p>
    <div class="buttons">
      <button class="danger" @click="${this.onDeleteConfirmClick_}">${
          this.strings.delete}</button>
      <button @click="${this.onDeleteCancelClick_}">${this.strings.cancel}</button>
    </div>
  </div>` : nothing}
<div id="switcher">
  ${this.spaces_.map(space => html`
    <button class="space-button" ?current="${space.current}"
        title="${space.name}" data-space="${space.id}" draggable="true"
        @click="${this.onSpaceClick_}"
        @contextmenu="${this.onSpaceContextmenu_}"
        @dragstart="${this.onSpaceDragstart_}"
        @dragover="${this.onSpaceDragover_}"
        @drop="${this.onSpaceDrop_}">
      <span class="svg" style="mask-image: ${this.iconMask_(space.icon)}"
          ?hidden="${!this.iconMask_(space.icon)}"></span>
      ${this.iconText_(space.icon)}
    </button>`)}
  <button id="add-space" class="space-button" title="${this.strings.newSpace}"
      ?hidden="${!this.spaces_.length}"
      @click="${this.onAddSpaceClick_}">＋</button>
  <button id="library-button" class="space-button"
      title="${this.strings.library}" ?selected="${this.libraryOpen_}"
      @click="${this.onLibraryClick_}">🗃</button>
</div>
${this.addProgress_ > 0 ? html`
  <div id="add-swipe" ?ready="${this.addProgress_ >= 1}"
      style="${this.addSwipeStyle_()}">
    <svg viewBox="0 0 36 36">
      <circle class="track" cx="18" cy="18" r="16" pathLength="100"></circle>
      <circle class="arc" cx="18" cy="18" r="16" pathLength="100"
          stroke-dasharray="${this.addSwipeArc_()}"></circle>
    </svg>
    <span>＋</span>
  </div>` : nothing}
${this.libraryShown_ ? html`
  <cromarc-library id="library" ?dragging="${this.libraryDrag_ >= 0}"
      .spaces="${this.spaces_}"
      style="${this.libraryStyle_()}"
      @library-close="${this.onLibraryClose_}">
  </cromarc-library>` : nothing}
${this.cleared_.length ? html`
  <div id="toast" role="status">
    <span>${this.strings.cleared}</span>
    <button class="text" @click="${this.onUndoClearClick_}">${
        this.strings.undo}</button>
  </div>` : nothing}
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
