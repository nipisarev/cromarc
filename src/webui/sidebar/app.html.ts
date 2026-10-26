// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html, nothing, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {SidebarAppElement} from './app.js';
import {essentialsLayout} from './sidebar_model.js';

export function getHtml(this: SidebarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="toolbox" style="${this.toolboxStyle_()}">
<div id="viewport" ?editing="${!!this.editor_}"
    ?stack-open="${this.stackOpen_}"
    ?stack-closing="${this.stackClosing_}">
  ${this.strips_().map(({space, content: strip}, index) => html`
  <div class="strip" style="${this.stripStyle_(index)}">
    <div id="${strip.live ? 'sidebar' : nothing}"
          class="sidebar ${strip.live ? '' : 'snapshot'}" ?inert="${!strip.live}"
          @contextmenu="${this.onBackgroundContextmenu_}">
      <div id="${strip.live ? 'essentials' : nothing}" class="essentials"
          layout="${essentialsLayout(strip.view.essentials.length)}"
          @dragover="${this.onEssentialsDragover_}"
          @drop="${this.onEssentialsDrop_}">
        ${strip.live && this.essentialsPromo_() ? html`
          <div class="essentials-promo ${this.promoOver_ ? 'over' : ''}"
              @dragover="${this.onPromoDragover_}"
              @dragleave="${this.onPromoDragleave_}">
            <span class="svg" style="mask-image: ${this.iconMask_('icon:heart')}">
            </span>
            <span class="promo-title">${this.strings.essentialsPromoLabel}</span>
            <span class="promo-sublabel">${
                this.strings.essentialsPromoSublabel}</span>
          </div>` : html`${
          repeat(strip.live ? this.essentialCells_() : strip.view.essentials, e => e?.id ?? '', e => e ? html`
          <div class="essential ${this.isDragged_(e.id) ? 'dragging' : ''}"
              data-id="${e.id}" data-kind="item"
              draggable="true" title="${e.title}" state="${e.state}"
              ?active="${this.isActive_(e.tab)}"
              style="--essential-icon: ${this.faviconFor_(e.homeUrl, e.tab, strip.favicons)}"
              @click="${this.onItemClick_}"
              @contextmenu="${this.onItemContextmenu_}"
              @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(e.homeUrl, e.tab, strip.favicons)}">
            </span>
          </div>` : html`<div class="essential slot"></div>`)}`}
      </div>
      <div class="space-header ${
          strip.live && this.dropZone_ === 'header' ? 'over' : ''}"
          ?has-pins="${strip.view.rows.length > 0}"
          ?collapsed="${strip.live && this.pinsCollapsed_}"
          @click="${this.onHeaderClick_}"
          @dragover="${this.onHeaderDragover_}"
          @dragleave="${this.onZoneDragleave_}"
          @drop="${this.onHeaderDrop_}">
        <span class="space-icon-stack" ?no-icon="${!space.icon}">
        <span class="svg chevron"
            style="mask-image: url(sidebar/ui_icons/chevron.svg)"></span>
        <span class="space-icon" ?no-icon="${!space.icon}">${
            this.iconMask_(space.icon) ? html`
            <span class="svg" style="mask-image: ${this.iconMask_(space.icon)}">
            </span>` : html`${space.icon}`}</span>
        </span>
        <span class="space-name" @dblclick="${this.onSpaceNameDblclick_}">${
            space.name}</span>
        <button class="space-menu-button" title="${this.strings.spaceActions}"
            @click="${this.onSpaceMenuClick_}">
          <span class="svg" style="mask-image: url(sidebar/ui_icons/menu.svg)">
          </span>
        </button>
      </div>
      <div id="${strip.live ? 'tabs' : nothing}" class="tabs">
      <div id="${strip.live ? 'tree' : nothing}" class="tree" role="tree">
        ${(strip.live ? this.visibleRows_() : strip.view.rows).map(row => row.kind === 'folder' ? html`
          <div class="row folder ${this.dropClass_(row.id)}" data-id="${row.id}"
              data-kind="item" draggable="true" style="--depth: ${row.depth}"
              ?collapsed="${row.collapsed}"
              @click="${this.onFolderClick_}"
              @contextmenu="${this.onItemContextmenu_}"
              @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}"
              @dragover="${this.onRowDragover_}" @drop="${this.onRowDrop_}">
            <cromarc-folder-icon ?open="${!row.collapsed}"></cromarc-folder-icon>
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
                    style="background-image: ${this.faviconFor_(row.homeUrl, null, strip.favicons)}">
                </span>
              </button>` : nothing}
            <span class="favicon current"
                style="background-image: ${this.faviconFor_(row.homeUrl, row.tab, strip.favicons)}">
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
                  data-tab="${row.tab.id}" @click="${this.onCloseTabClick_}">
                <span class="svg"
                    style="mask-image: url(sidebar/ui_icons/close.svg)"></span>
              </button>` : nothing}
          </div>`)}
        <div id="${strip.live ? 'tree-end' : nothing}" class="tree-end ${
            strip.live && this.dropZone_ === 'end' ? 'over' : ''}"
            @dragover="${this.onTreeEndDragover_}"
            @dragleave="${this.onZoneDragleave_}"
            @drop="${this.onTreeEndDrop_}"></div>
      </div>
      <div class="separator" ?hidden="${strip.view.today.length === 0}"
          @dragover="${this.onTreeEndDragover_}"
          @dragleave="${this.onZoneDragleave_}"
          @drop="${this.onTreeEndDrop_}">
        <span class="line"></span>
        <button class="clear" title="${this.strings.clearTooltip}"
            @click="${this.onClearClick_}">
          <span class="svg" style="mask-image: url(sidebar/ui_icons/clear.svg)">
          </span>
          <span>${this.strings.clear}</span>
        </button>
      </div>
      ${strip.view.attention.length ? html`
        <div class="section-header attention">⚠ ${this.strings.needsAttention}</div>
        ${strip.view.attention.map(tab => html`
          <div class="row attention" data-id="${tab.id}" title="${tab.title}"
              ?active="${tab.isActive}" @click="${this.onTodayClick_}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(tab.url, tab, strip.favicons)}">
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
      <div class="row new-tab" @click="${this.onNewTabClick_}">
        <span class="svg" style="mask-image: url(sidebar/ui_icons/add.svg)">
        </span>
        <span class="title">${this.strings.newTab}</span>
      </div>
      <div id="${strip.live ? 'today' : nothing}" class="today-list"
          @dragover="${this.onTodayDragover_}"
          @drop="${this.onTodayDrop_}">
        ${strip.view.today.map(tab => html`
          <div class="row today" data-id="${tab.id}" data-kind="tab"
              draggable="true" ?active="${tab.isActive}"
              title="${this.todayTooltip_(tab)}"
              @click="${this.onTodayClick_}"
              @contextmenu="${this.onTodayContextmenu_}"
              @dragstart="${this.onDragstart_}" @dragend="${this.onDragend_}">
            <span class="favicon"
                style="background-image: ${this.faviconFor_(tab.url, tab, strip.favicons)}">
            </span>
            <span class="title">${tab.title || tab.url}</span>
            <button class="close" title="${this.strings.close}"
                data-tab="${tab.id}" @click="${this.onCloseTabClick_}">
              <span class="svg"
                  style="mask-image: url(sidebar/ui_icons/close.svg)"></span>
            </button>
            <span class="countdown" style="width: ${this.countdown_(tab)}"></span>
          </div>`)}
      </div>
      </div>
    </div>
  </div>`)}
</div>
${this.editor_ ? html`
  <cromarc-space-editor id="space-editor" ?creating="${!this.editor_.id}"
      .name="${this.editor_.name}" .icon="${this.editor_.icon}"
      .pickerOpen="${this.editor_.pickerOpen}"
      .remoteSpaces="${this.remoteSpaces_}"
      @space-connect="${this.onSpaceConnect_}"
      @space-edit-done="${this.onSpaceEditDone_}"
      @space-edit-cancel="${this.onSpaceEditCancel_}"
      @space-edit-theme="${this.onSpaceEditTheme_}">
  </cromarc-space-editor>` : nothing}
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
<div id="foot" ?stack-open="${this.stackOpen_}"
    ?stack-closing="${this.stackClosing_}" ?stack-badge="${this.stackBadge_}">
  <div id="download-list" @mouseenter="${this.onDownloadListMouseenter_}"
      @mouseleave="${this.onStackMouseleave_}">
    ${this.stackEntries_().map(download => html`
      <div class="download-entry" draggable="true" ?hidden="${!download}"
          data-id="${download?.id ?? ''}" title="${download?.fileName ?? ''}"
          ?downloading="${this.downloadPending_(download)}"
          ?show-in-folder="${this.downloadHasFile_(download)}"
          @click="${this.onDownloadEntryClick_}"
          @contextmenu="${this.onDownloadEntryContextmenu_}"
          @dragstart="${this.onDownloadEntryDragstart_}">
        <span class="download-badge"
            ?downloading="${this.downloadPending_(download)}"
            style="${this.downloadImage_(download)}">
          <svg class="download-progress" viewBox="0 0 31 31">
            <circle class="download-progress-track" cx="15.5" cy="15.5" r="14">
            </circle>
            <circle class="download-progress-arc" cx="15.5" cy="15.5" r="14"
                pathLength="100"
                style="stroke-dasharray: ${this.downloadDash_(download)}">
            </circle>
          </svg>
        </span>
        <div class="download-entry-text">
          <span class="download-entry-title">${download?.fileName ?? ''}</span>
          <span class="download-entry-subtitle">${
              this.downloadStatus_(download)}</span>
        </div>
        <button class="download-action" data-id="${download?.id ?? ''}"
            title="${this.downloadActionLabel_(download)}"
            @click="${this.onDownloadActionClick_}">
          <span class="svg" style="mask-image: url(sidebar/ui_icons/${
              this.downloadPending_(download) ? 'close' : 'search'}.svg)">
          </span>
        </button>
      </div>`)}
  </div>
  <button id="library-button" class="foot-button"
      title="${this.strings.library}" ?selected="${this.libraryOpen_}"
      @click="${this.onLibraryClick_}"
      @mouseenter="${this.onLibraryButtonMouseenter_}"
      @mouseleave="${this.onStackMouseleave_}">
    <span class="library-sprite"></span>
    <span class="badge-stack" ?downloading="${this.newestPending_()}">
      <span id="library-button-badge" class="download-badge"
          ?downloading="${this.newestPending_()}"
          style="${this.downloadImage_(this.stack_[this.stack_.length - 1])}">
        <svg class="download-progress" viewBox="0 0 31 31">
          <circle class="download-progress-track" cx="15.5" cy="15.5" r="14">
          </circle>
          <circle class="download-progress-arc" cx="15.5" cy="15.5" r="14"
              pathLength="100" style="stroke-dasharray: ${
                  this.downloadDash_(this.stack_[this.stack_.length - 1])}">
          </circle>
        </svg>
      </span>
    </span>
  </button>
  <div id="switcher">
    ${this.spaces_.map(space => html`
      <button class="space-button" ?current="${space.current}"
          title="${space.name}" data-space="${space.id}" draggable="true"
          @click="${this.onSpaceClick_}"
          @contextmenu="${this.onSpaceContextmenu_}"
          @dragstart="${this.onSpaceDragstart_}"
          @dragover="${this.onSpaceDragover_}"
          @drop="${this.onSpaceDrop_}">
        <span class="space-icon" ?no-icon="${!space.icon}">${
            this.iconMask_(space.icon) ? html`
            <span class="svg" style="mask-image: ${this.iconMask_(space.icon)}">
            </span>` : html`${space.icon}`}</span>
      </button>`)}
  </div>
  <button id="create-new" class="foot-button" title="${this.strings.createNew}"
      @click="${this.onCreateNewClick_}">
    <span class="svg" style="mask-image: url(sidebar/ui_icons/add.svg)"></span>
  </button>
</div>
${this.addProgress_ > 0 ? html`
  <div id="add-swipe" ?ready="${this.addProgress_ >= 1}"
      style="${this.addSwipeStyle_()}">
    <svg viewBox="0 0 36 36">
      <circle class="track" cx="18" cy="18" r="16" pathLength="100"></circle>
      <circle class="arc" cx="18" cy="18" r="16" pathLength="100"
          stroke-dasharray="${this.addSwipeArc_()}"></circle>
    </svg>
    <span class="svg" style="mask-image: url(sidebar/ui_icons/add.svg)"></span>
  </div>` : nothing}
${this.cleared_.length ? html`
  <div id="toast" role="status">
    <span>${this.strings.cleared}</span>
    <button class="text" @click="${this.onUndoClearClick_}">${
        this.strings.undo}</button>
  </div>` : nothing}
</div>
${this.libraryShown_ ? html`
  <cromarc-library id="library" ?open="${this.libraryOpen_}"
      .spaces="${this.spaces_}" .primary="${this.primary_}"
      style="${this.libraryStyle_()}"
      @library-close="${this.onLibraryClose_}"
      @library-close-now="${this.onLibraryCloseNow_}"
      @library-keep-open="${this.onLibraryKeepOpen_}"
      @library-width="${this.onLibraryWidth_}"
      @library-space-menu="${this.onLibrarySpaceMenu_}"
      @library-space-theme="${this.onLibrarySpaceTheme_}"
      @library-create-space="${this.onLibraryCreateSpace_}">
  </cromarc-library>` : nothing}
<div id="drag-ghost"></div>
<!--_html_template_end_-->`;
  // clang-format on
}

