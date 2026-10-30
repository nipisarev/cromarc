// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_LAST_SESSION_H_
#define CROMARC_BROWSER_PORTS_FAKE_LAST_SESSION_H_

#include <set>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "cromarc/browser/ports/last_session.h"

namespace cromarc {

// The last session of the profiles in `due`; it comes when the test calls
// Finish(), after adding the tabs itself.
class FakeLastSession : public LastSession {
 public:
  FakeLastSession();
  ~FakeLastSession() override;

  // LastSession:
  bool IsDue(Profile* profile) override;
  void RestoreInto(BrowserWindowInterface* window,
                   base::OnceCallback<void(size_t added)> done) override;

  // Answers the oldest RestoreInto().
  void Finish(size_t added);

  std::set<Profile*> due;
  // What RestoreInto() was asked for, in turn.
  std::vector<BrowserWindowInterface*> restored;

 private:
  std::vector<base::OnceCallback<void(size_t)>> pending_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_LAST_SESSION_H_
