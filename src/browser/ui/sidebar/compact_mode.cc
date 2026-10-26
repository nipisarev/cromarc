// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/compact_mode.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "cromarc/browser/ports/browser_frame.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ui/sidebar/compact_hover.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace cromarc {

namespace {

// Zen's compact mode timings (prefs/zen/compact-mode.yaml).
constexpr base::TimeDelta kKeepAfterLeave = base::Milliseconds(150);
constexpr base::TimeDelta kFlash = base::Milliseconds(800);
// How close to the window's left edge the cursor reveals the sidebar.
constexpr int kEdge = 6;

}  // namespace

CompactMode::CompactMode(VerticalStrip& strip,
                         BrowserFrame& frame,
                         Tabs& tabs,
                         BrowserWindowInterface* window,
                         MouseWatcher watch_mouse)
    : strip_(strip), frame_(frame), watch_mouse_(std::move(watch_mouse)) {
  opened_in_background_ = tabs.OnOpenedInBackground(
      window, base::BindRepeating(&CompactMode::Flash, base::Unretained(this)));
}

CompactMode::~CompactMode() = default;

bool CompactMode::Toggle() {
  if (!strip_->IsShown()) {
    return false;
  }
  // Collapsing counts as collapsed: Cmd+S during the animation turns back.
  strip_->RequestCollapse(!strip_->IsCollapsedOrCollapsing());
  return true;
}

bool CompactMode::IsCompact() const {
  return strip_->IsCollapsed();
}

void CompactMode::OnMouseMoved(const gfx::Point& screen_point) {
  const std::optional<gfx::Rect> strip = strip_->BoundsInScreen();
  if (!IsCompact() || !strip) {
    Hide();
    return;
  }
  const gfx::Rect window = frame_->BoundsInScreen();
  const bool in_strip_band = screen_point.y() >= strip->y() &&
                             screen_point.y() < strip->bottom();
  if (!hold_) {
    if (in_strip_band && screen_point.x() - window.x() <= kEdge) {
      Reveal();
    }
    return;
  }
  if (CompactSidebarStaysOut(window, *strip, screen_point)) {
    hide_timer_.Stop();
  } else if (!hide_timer_.IsRunning()) {
    HideAfter(kKeepAfterLeave);
  }
}

void CompactMode::Flash() {
  if (!IsCompact()) {
    return;
  }
  Reveal();
  HideAfter(kFlash);
}

void CompactMode::Reveal() {
  hide_timer_.Stop();
  if (hold_) {
    return;
  }
  hold_ = strip_->KeepOut();
  if (hold_) {
    mouse_watch_ = watch_mouse_.Run(base::BindRepeating(
        &CompactMode::OnMouseMoved, weak_factory_.GetWeakPtr()));
  }
}

void CompactMode::HideAfter(base::TimeDelta delay) {
  hide_timer_.Start(FROM_HERE, delay,
                    base::BindOnce(&CompactMode::Hide, base::Unretained(this)));
}

void CompactMode::Hide() {
  hide_timer_.Stop();
  hold_.reset();
  // Not from inside the monitor's own handler.
  if (mouse_watch_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(mouse_watch_)));
  }
}

}  // namespace cromarc
