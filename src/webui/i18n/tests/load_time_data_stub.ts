// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// //resources/js/load_time_data.js for node tests: the same lookups and the
// same substitution as Chromium's, and the same failure on a missing key.
class LoadTimeData {
  private data_: Record<string, string> = {};

  set data(value: Record<string, string>) {
    this.data_ = value;
  }

  valueExists(id: string): boolean {
    return id in this.data_;
  }

  getString(id: string): string {
    if (!(id in this.data_)) {
      throw new Error(`Could not find value for ${id}`);
    }
    return this.data_[id]!;
  }

  getStringF(id: string, ...args: Array<string|number>): string {
    const value = this.getString(id);
    return value ? this.substituteString(value, ...args) : '';
  }

  substituteString(label: string, ...args: Array<string|number>): string {
    return label.replace(/\$(.|$|\n)/g, m => {
      if (!/\$[$1-9]/.test(m)) {
        throw new Error('Unescaped $ found in localized string.');
      }
      if (m === '$$') {
        return '$';
      }
      const substitute = args[Number(m[1]) - 1];
      return substitute === undefined || substitute === null ? '' :
                                                             substitute.toString();
    });
  }
}

export const loadTimeData = new LoadTimeData();
