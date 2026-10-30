// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_browser_frame.h"

#include <utility>

namespace cromarc {

FakeBrowserFrame::FakeBrowserFrame() = default;
FakeBrowserFrame::~FakeBrowserFrame() = default;

void FakeBrowserFrame::UserTogglesFullscreen() {
  on_fullscreen_.Notify();
}

gfx::Rect FakeBrowserFrame::BoundsInScreen() const {
  return bounds;
}

void FakeBrowserFrame::RepaintBackground() {
  ++background_repaints;
}

void FakeBrowserFrame::UpdatePageCorners() {
  ++page_corner_updates;
}

base::CallbackListSubscription FakeBrowserFrame::OnFullscreenChanged(
    base::RepeatingClosure callback) {
  return on_fullscreen_.Add(std::move(callback));
}

void FakeBrowserFrame::AttachWebUI(content::WebContents* contents) {}

gfx::Size FakeBrowserFrame::WindowButtons() const {
  return window_buttons;
}

void FakeBrowserFrame::ShowNavigationHistory(
    bool forward,
    const gfx::Point& point_in_screen) {
  history_menus.emplace_back(forward, point_in_screen);
}

}  // namespace cromarc
