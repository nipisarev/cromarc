// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_glance_surface.h"

#include <utility>

namespace cromarc {

FakeGlanceSurface::FakeGlanceSurface() = default;
FakeGlanceSurface::~FakeGlanceSurface() = default;

void FakeGlanceSurface::FinishHide(BrowserWindowInterface* window) {
  auto it = hiding_.find(window);
  if (it == hiding_.end()) {
    return;
  }
  base::OnceClosure done = std::move(it->second);
  hiding_.erase(it);
  if (done) {
    std::move(done).Run();
  }
}

void FakeGlanceSurface::Snapshot(TabId tab) {
  snapshots.insert(tab);
}

void FakeGlanceSurface::Forget(TabId tab) {
  snapshots.erase(tab);
}

void FakeGlanceSurface::Show(BrowserWindowInterface* window,
                             TabId glance,
                             TabId parent,
                             std::optional<gfx::Point> origin,
                             Client* shown_client) {
  client = shown_client;
  shown[window] = {glance, parent, origin};
  shows.push_back({glance, parent, origin});
}

void FakeGlanceSurface::Hide(BrowserWindowInterface* window,
                             bool animate,
                             base::OnceClosure done) {
  shown.erase(window);
  if (animate) {
    hiding_[window] = std::move(done);
  } else if (done) {
    std::move(done).Run();
  }
}

void FakeGlanceSurface::AskToConfirmClose(BrowserWindowInterface* window,
                                          base::TimeDelta duration) {
  confirm_asks.push_back(window);
}

}  // namespace cromarc
