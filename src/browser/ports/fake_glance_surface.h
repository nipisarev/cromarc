// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_GLANCE_SURFACE_H_
#define CROMARC_BROWSER_PORTS_FAKE_GLANCE_SURFACE_H_

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/glance_surface.h"

namespace cromarc {

// Records what would show; a hide with animation waits for FinishHide().
class FakeGlanceSurface : public GlanceSurface {
 public:
  struct Shown {
    TabId glance;
    TabId parent;
    std::optional<gfx::Point> origin;
  };

  FakeGlanceSurface();
  ~FakeGlanceSurface() override;

  // Ends the closing animation of `window`.
  void FinishHide(BrowserWindowInterface* window);

  // GlanceSurface:
  void Snapshot(TabId tab) override;
  void Forget(TabId tab) override;
  void Show(BrowserWindowInterface* window,
            TabId glance,
            TabId parent,
            std::optional<gfx::Point> origin,
            Client* client) override;
  void Hide(BrowserWindowInterface* window,
            bool animate,
            base::OnceClosure done) override;
  void AskToConfirmClose(BrowserWindowInterface* window,
                         base::TimeDelta duration) override;

  // What each window shows now.
  std::map<BrowserWindowInterface*, Shown> shown;
  // Every Show(), in order.
  std::vector<Shown> shows;
  std::set<TabId> snapshots;
  // Every AskToConfirmClose(), in order.
  std::vector<BrowserWindowInterface*> confirm_asks;
  raw_ptr<Client> client = nullptr;

 private:
  std::map<BrowserWindowInterface*, base::OnceClosure> hiding_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_GLANCE_SURFACE_H_
