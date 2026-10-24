// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ArcImportAppElement} from './app.js';

export function getHtml(this: ArcImportAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<main>
  <h1>Перенести из Arc</h1>
  <p class="hint" ?hidden="${this.loading_ || !this.found_ || this.fromExport_}">
    Папки, закреплённые и важные вкладки, вкладки Today (откроются,
    не загружаясь). Можно запускать снова: уже перенесённое не дублируется.
  </p>
  <p class="hint" ?hidden="${!this.fromExport_}">
    Из файла экспорта Arc: в нём только ссылки. Favorites закрепятся в корне
    space, названия вкладок появятся, когда вы их откроете.
  </p>
  <p ?hidden="${!this.loading_}">Читаю сайдбар Arc…</p>
  <p id="missing" ?hidden="${this.loading_ || this.found_}">
    Не нашёл данные Arc
    (~/Library/Application Support/Arc/StorableSidebar.json). Если Arc уже
    удалён, перенесите из его файла экспорта: Arc → Settings → Advanced →
    Export Data.
  </p>
  <p ?hidden="${this.loading_}">
    <button id="choose-export" class="${this.found_ ? 'link' : ''}"
        ?disabled="${this.importing_}" @click="${this.onChooseExportClick_}">
      ${this.found_ ? 'Или из файла экспорта Arc…' :
                      'Выбрать файл экспорта…'}
    </button>
    <span id="export-failed" ?hidden="${!this.exportFailed_}">
      Файл не выбран или это не экспорт Arc.
    </span>
  </p>
  <div id="spaces" ?hidden="${!this.found_}">
    ${this.spaces_.map(space => html`
      <div class="space">
        <span class="icon">${this.iconOf_(space)}</span>
        <span class="text">
          <span class="title">${space.title}</span>
          <span class="counts">${this.countsOf_(space)}</span>
        </span>
        <select data-arc-id="${space.arcId}" ?disabled="${this.importing_}"
            @change="${this.onTargetChange_}">
          ${this.targets_.map(target => html`
            <option value="${target.id}"
                ?selected="${this.isChosen_(space.arcId, target.id)}">
              → ${this.targetLabel_(target)}
            </option>`)}
          <option value="${this.newSpace_}"
              ?selected="${this.isChosen_(space.arcId, this.newSpace_)}">
            → Новый space
          </option>
          <option value="" ?selected="${this.isChosen_(space.arcId, '')}">
            Не переносить
          </option>
        </select>
      </div>`)}
    <button id="import" ?disabled="${this.importing_}"
        @click="${this.onImportClick_}">
      ${this.importing_ ? 'Переношу…' : 'Перенести'}
    </button>
  </div>
  <pre id="summary" ?hidden="${!this.summary_}">${this.summary_}</pre>
</main>
<!--_html_template_end_-->`;
  // clang-format on
}
