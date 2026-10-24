// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FolderIconElement} from './folder_icon.js';

export function getHtml(this: FolderIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<svg width="28" height="28" viewBox="0 0 27 27" fill="none">
  <defs>
    <linearGradient gradientUnits="userSpaceOnUse" x1="14" y1="5.625" x2="14" y2="22.375" id="gradient-0">
      <stop offset="0" style="stop-color: rgb(255, 255, 255)"/>
      <stop offset="1" style="stop-color: rgb(0% 0% 0%)"/>
    </linearGradient>
    <linearGradient gradientUnits="userSpaceOnUse" x1="14" y1="9.625" x2="14" y2="22.375" id="gradient-1">
      <stop offset="0" style="stop-color: rgb(255, 255, 255)"/>
      <stop offset="1" style="stop-color: rgb(0% 0% 0%)"/>
    </linearGradient>
  </defs>
  <path class="back" d="M8 5.625H11.9473C12.4866 5.625 13.0105 5.80861 13.4316 6.14551L14.2881 6.83105C14.9308 7.34508 15.7298 7.625 16.5527 7.625H20C21.3117 7.625 22.375 8.68832 22.375 10V20C22.375 21.3117 21.3117 22.375 20 22.375H8C6.68832 22.375 5.625 21.3117 5.625 20V8C5.625 6.68832 6.68832 5.625 8 5.625Z" style="fill: var(--zen-folder-behind-bgcolor);"></path>
  <path class="back" d="M8 5.625H11.9473C12.4866 5.625 13.0105 5.80861 13.4316 6.14551L14.2881 6.83105C14.9308 7.34508 15.7298 7.625 16.5527 7.625H20C21.3117 7.625 22.375 8.68832 22.375 10V20C22.375 21.3117 21.3117 22.375 20 22.375H8C6.68832 22.375 5.625 21.3117 5.625 20V8C5.625 6.68832 6.68832 5.625 8 5.625Z" style="stroke-width: 1.5px; stroke: var(--zen-folder-stroke); fill: url(#gradient-0); fill-opacity: 0.1;"></path>
  <rect class="front" x="5.625" y="9.625" width="16.75" height="12.75" rx="2.375" style="fill: var(--zen-folder-front-bgcolor);"></rect>
  <rect class="front" x="5.625" y="9.625" width="16.75" height="12.75" rx="2.375" style="stroke-width: 1.5px; stroke: var(--zen-folder-stroke); fill: url(#gradient-1); fill-opacity: 0.1;"></rect>
</svg>
<!--_html_template_end_-->`;
  // clang-format on
}
