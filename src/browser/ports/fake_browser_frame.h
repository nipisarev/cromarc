// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_BROWSER_FRAME_H_
#define CROMARC_BROWSER_PORTS_FAKE_BROWSER_FRAME_H_

#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "cromarc/browser/ports/browser_frame.h"

namespace cromarc {

class FakeBrowserFrame : public BrowserFrame {
 public:
  FakeBrowserFrame();
  ~FakeBrowserFrame() override;

  void UserTogglesFullscreen();

  // BrowserFrame:
  gfx::Rect BoundsInScreen() const override;
  void RepaintBackground() override;
  void UpdatePageCorners() override;
  base::CallbackListSubscription OnFullscreenChanged(
      base::RepeatingClosure callback) override;
  void AttachWebUI(content::WebContents* contents) override;
  gfx::Size WindowButtons() const override;
  void ShowNavigationHistory(bool forward,
                             const gfx::Point& point_in_screen) override;
  void PlaceExtensions(const gfx::Rect& bounds_in_screen) override;
  int ExtensionsWidth() const override;
  base::CallbackListSubscription OnExtensionsWidthChanged(
      base::RepeatingClosure callback) override;

  gfx::Rect bounds = gfx::Rect(0, 0, 1100, 800);
  // macOS's traffic lights with Chromium's padding.
  gfx::Size window_buttons = gfx::Size(78, 38);
  // (forward, point) of each history menu shown.
  std::vector<std::pair<bool, gfx::Point>> history_menus;
  gfx::Rect extensions_bounds;
  int extensions_width = 0;
  base::RepeatingClosureList extensions_width_changed;
  int background_repaints = 0;
  int page_corner_updates = 0;

 private:
  base::RepeatingClosureList on_fullscreen_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_BROWSER_FRAME_H_
