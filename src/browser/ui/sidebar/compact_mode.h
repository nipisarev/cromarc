// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_
#define CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "cromarc/browser/ports/vertical_strip.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace cromarc {

class BrowserFrame;
class Tabs;

// Compact mode (design, UI section 9), Zen's compact mode with the sidebar
// hidden: Chrome's collapsed vertical tab strip has no width in Cromarc (hook
// H3), so collapsing leaves the page the whole window under the toolbar. The
// sidebar slides out over the page when the cursor reaches the window's left
// edge and goes 150 ms after it leaves (zen.view.compact.sidebar-keep-hover),
// and flashes for 800 ms when a tab opens in the background (Zen's
// flashSidebar). Hidden, nothing of the strip shows, as Zen's sidebar is off
// the window then.
class CompactMode {
 public:
  // WatchMouseEverywhere() (space_input.h), or a fake in tests.
  using MouseWatcher = base::RepeatingCallback<base::ScopedClosureRunner(
      base::RepeatingCallback<void(const gfx::Point&)>)>;

  CompactMode(VerticalStrip& strip,
              BrowserFrame& frame,
              Tabs& tabs,
              MouseWatcher watch_mouse);
  CompactMode(const CompactMode&) = delete;
  CompactMode& operator=(const CompactMode&) = delete;
  ~CompactMode();

  // Cmd+S (Arc) and Ctrl+Alt+C (Zen). False when the window shows no
  // vertical tabs.
  bool Toggle();

  bool IsCompact() const;
  // The cursor is at `screen_point` over this window.
  void OnMouseMoved(const gfx::Point& screen_point);
  void Flash();

 private:
  void Reveal();
  void HideAfter(base::TimeDelta delay);
  void Hide();

  const raw_ref<VerticalStrip> strip_;
  const raw_ref<BrowserFrame> frame_;
  const MouseWatcher watch_mouse_;
  std::unique_ptr<VerticalStrip::Hold> hold_;
  // The cursor everywhere while the sidebar is out, as Zen's mouse tracker:
  // a move out of the window ends it, unless just past the sidebar's edge.
  base::ScopedClosureRunner mouse_watch_;
  base::OneShotTimer hide_timer_;
  base::CallbackListSubscription opened_in_background_;
  base::WeakPtrFactory<CompactMode> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_
