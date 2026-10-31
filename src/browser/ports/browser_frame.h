// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_BROWSER_FRAME_H_
#define CROMARC_BROWSER_PORTS_BROWSER_FRAME_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

// One browser window's frame: its bounds, its background and the page
// inside it.
class BrowserFrame {
 public:
  virtual ~BrowserFrame() = default;

  virtual gfx::Rect BoundsInScreen() const = 0;
  // Paints the window's background (its space theme) again.
  virtual void RepaintBackground() = 0;
  // Rounds the page's corners when it is a card on the background, squares
  // them when not (tab fullscreen).
  virtual void UpdatePageCorners() = 0;
  // Called when the window enters or leaves fullscreen. Empty until the
  // window's features exist, which is after its views.
  virtual base::CallbackListSubscription OnFullscreenChanged(
      base::RepeatingClosure callback) = 0;
  // Gives `contents`, a WebUI of the window's own chrome, its window, as
  // Chrome's top chrome WebUIs have it.
  virtual void AttachWebUI(content::WebContents* contents) = 0;
  // The room the window's controls (the traffic lights) take at its top left,
  // padding included; empty in fullscreen.
  virtual gfx::Size WindowButtons() const = 0;
  // Chromium's menu of the active tab's back or forward history at
  // `point_in_screen`.
  virtual void ShowNavigationHistory(bool forward,
                                     const gfx::Point& point_in_screen) = 0;
  // Shows the window's extension buttons at `bounds_in_screen`, where Zen's
  // single toolbar has them (the end of the address field); empty hides them.
  // Nothing in a window that keeps Chromium's toolbar.
  virtual void PlaceExtensions(const gfx::Rect& bounds_in_screen) = 0;
  virtual int ExtensionsWidth() const = 0;
  virtual base::CallbackListSubscription OnExtensionsWidthChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_BROWSER_FRAME_H_
