// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_LAST_SESSION_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_LAST_SESSION_ADAPTER_H_

#include "cromarc/browser/ports/last_session.h"

namespace cromarc {

// LastSession on Chromium's SessionService, adding tabs as its session
// restore does.
class LastSessionAdapter : public LastSession {
 public:
  LastSessionAdapter();
  ~LastSessionAdapter() override;

  // LastSession:
  bool IsDue(Profile* profile) override;
  void RestoreInto(BrowserWindowInterface* window,
                   base::OnceCallback<void(size_t added)> done) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_LAST_SESSION_ADAPTER_H_
