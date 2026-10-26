// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_VERTICAL_STRIP_H_
#define CROMARC_BROWSER_PORTS_VERTICAL_STRIP_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/geometry/rect.h"

namespace cromarc {

// One window's vertical tab strip, Chrome's column the sidebar lives in.
// Collapsed, it is Cromarc's compact mode.
class VerticalStrip {
 public:
  // While alive, the collapsed strip stays out over the page.
  class Hold {
   public:
    virtual ~Hold() = default;
  };

  virtual ~VerticalStrip() = default;

  // The window shows vertical tabs.
  virtual bool IsShown() const = 0;
  // The state asked for last, whether or not its animation is done.
  virtual bool IsCollapsed() const = 0;
  // Collapsed, or animating there.
  virtual bool IsCollapsedOrCollapsing() const = 0;
  virtual void RequestCollapse(bool collapse) = 0;

  // The width when not collapsed.
  virtual int ExpandedWidth() const = 0;
  virtual void SetExpandedWidth(int width) = 0;
  // The user is dragging its edge.
  virtual bool IsResizing() const = 0;

  virtual base::CallbackListSubscription OnCollapseChanged(
      base::RepeatingClosure callback) = 0;
  virtual base::CallbackListSubscription OnResizeEnded(
      base::RepeatingClosure callback) = 0;

  // Its bounds on screen; nullopt while it is not in a window.
  virtual std::optional<gfx::Rect> BoundsInScreen() const = 0;
  // Null while it is not in a window.
  virtual std::unique_ptr<Hold> KeepOut() = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_VERTICAL_STRIP_H_
