// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_VERTICAL_STRIP_H_
#define CROMARC_BROWSER_PORTS_FAKE_VERTICAL_STRIP_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "cromarc/browser/ports/vertical_strip.h"

namespace cromarc {

// A strip whose collapse takes effect at once, as when no animation runs.
class FakeVerticalStrip : public VerticalStrip {
 public:
  FakeVerticalStrip();
  ~FakeVerticalStrip() override;

  // What the user does to it.
  void UserCollapses(bool collapse);
  void UserResizes(int width);

  // VerticalStrip:
  bool IsShown() const override;
  bool IsCollapsed() const override;
  bool IsCollapsedOrCollapsing() const override;
  void RequestCollapse(bool collapse) override;
  int ExpandedWidth() const override;
  void SetExpandedWidth(int width) override;
  bool IsResizing() const override;
  base::CallbackListSubscription OnCollapseChanged(
      base::RepeatingClosure callback) override;
  base::CallbackListSubscription OnResizeEnded(
      base::RepeatingClosure callback) override;
  std::optional<gfx::Rect> BoundsInScreen() const override;
  std::unique_ptr<Hold> KeepOut() override;

  bool shown = true;
  bool collapsed = false;
  bool collapsing = false;
  int expanded_width = 240;
  bool resizing = false;
  std::optional<gfx::Rect> bounds = gfx::Rect(0, 40, 240, 760);
  // How many holds are alive.
  int holds = 0;
  std::vector<bool> collapse_requests;

 private:
  base::RepeatingClosureList on_collapse_;
  base::RepeatingClosureList on_resize_ended_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_VERTICAL_STRIP_H_
