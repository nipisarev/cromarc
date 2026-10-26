// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_LOCAL_SYNC_H_
#define CROMARC_BROWSER_PORTS_LOCAL_SYNC_H_

namespace base {
class FilePath;
}  // namespace base

namespace cromarc {

// Chrome's sync of one profile to a file of its own (the local sync backend)
// instead of Google's servers.
class LocalSync {
 public:
  virtual ~LocalSync() = default;

  // Whether the profile syncs to a file from the next start on.
  virtual bool IsEnabled() = 0;
  // Whether it syncs to a file now.
  virtual bool IsActive() = 0;
  // From the next start on, to `file`; the sync service reads it when the
  // profile loads.
  virtual void Enable(const base::FilePath& file) = 0;
  virtual void Disable() = 0;
  // Chrome's sync wants a finished first-run setup and the data types to
  // carry: everything but what a password manager or the Mac keeps. Done
  // once, while active.
  virtual void SetUpOnce() = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_LOCAL_SYNC_H_
