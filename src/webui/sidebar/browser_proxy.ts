// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './sidebar.mojom-webui.js';
import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';

// The sidebar's two connections to the browser: Cromarc's tree service and
// Chromium's TabStripService for this window.
export class BrowserProxy {
  readonly handler: PageHandlerRemote = new PageHandlerRemote();
  readonly callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  readonly tabStrip: TabStripServiceRemote = TabStripService.getRemote();

  constructor() {
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
