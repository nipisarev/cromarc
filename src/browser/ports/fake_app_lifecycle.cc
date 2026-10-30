// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_app_lifecycle.h"

#include <utility>

namespace cromarc {

FakeAppLifecycle::FakeAppLifecycle() = default;
FakeAppLifecycle::~FakeAppLifecycle() = default;

void FakeAppLifecycle::CloseAllWindows(bool closing) {
  closing_all_.Notify(closing);
}

bool FakeAppLifecycle::IsRestoringSession(Profile* profile) {
  return restoring;
}

void FakeAppLifecycle::Restart() {
  ++restarts;
}

bool FakeAppLifecycle::IsSessionEnding() {
  return session_ending;
}

base::CallbackListSubscription FakeAppLifecycle::OnClosingAllWindows(
    base::RepeatingCallback<void(bool closing)> callback) {
  return closing_all_.Add(std::move(callback));
}

}  // namespace cromarc
