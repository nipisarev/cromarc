// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import type {Download} from '../sidebar.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';

type Listener = (downloads: Download[]) => void;

// The profile's downloads, oldest first, for the Library's downloads and the
// download stack on the library button (Zen's DownloadsCommon data and its
// views). Read again as the browser says they changed.
class DownloadsStore {
  private downloads_: Download[] = [];
  private loaded_ = false;
  private reading_ = false;
  private again_ = false;
  private readonly listeners_ = new Set<Listener>();
  private readonly proxy_ = BrowserProxy.getInstance();

  constructor() {
    this.proxy_.callbackRouter.onDownloadsChanged.addListener(
        () => this.read_());
    this.read_();
  }

  // Calls `listener` now if the list is in, and on every change; returns
  // what stops it.
  subscribe(listener: Listener): () => void {
    this.listeners_.add(listener);
    if (this.loaded_) {
      listener(this.downloads_);
    }
    return () => this.listeners_.delete(listener);
  }

  private async read_() {
    if (this.reading_) {
      this.again_ = true;
      return;
    }
    this.reading_ = true;
    const {downloads} = await this.proxy_.handler.getDownloads();
    this.reading_ = false;
    this.downloads_ = downloads;
    this.loaded_ = true;
    for (const listener of this.listeners_) {
      listener(downloads);
    }
    if (this.again_) {
      this.again_ = false;
      this.read_();
    }
  }
}

let store: DownloadsStore|null = null;

export function downloadsStore(): DownloadsStore {
  store ??= new DownloadsStore();
  return store;
}
