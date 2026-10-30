// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_last_session.h"

namespace cromarc {

FakeLastSession::FakeLastSession() = default;
FakeLastSession::~FakeLastSession() = default;

bool FakeLastSession::IsDue(Profile* profile) {
  return due.contains(profile);
}

void FakeLastSession::RestoreInto(BrowserWindowInterface* window,
                                  base::OnceCallback<void(size_t added)> done) {
  restored.push_back(window);
  pending_.push_back(std::move(done));
}

void FakeLastSession::Finish(size_t added) {
  base::OnceCallback<void(size_t)> done = std::move(pending_.front());
  pending_.erase(pending_.begin());
  std::move(done).Run(added);
}

}  // namespace cromarc
