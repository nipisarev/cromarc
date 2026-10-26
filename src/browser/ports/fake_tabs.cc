// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_tabs.h"

#include <utility>

namespace cromarc {

FakeTabs::FakeTabs() = default;
FakeTabs::~FakeTabs() = default;

void FakeTabs::OpenInBackground() {
  on_opened_in_background_.Notify();
}

base::CallbackListSubscription FakeTabs::OnOpenedInBackground(
    base::RepeatingClosure callback) {
  return on_opened_in_background_.Add(std::move(callback));
}

}  // namespace cromarc
