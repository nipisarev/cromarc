// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_
#define CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"  // nogncheck: //chrome/browser/ui links //cromarc

class BrowserWindowInterface;
class ExpandOnHoverLock;
class VerticalTabStripRegionView;

namespace gfx {
class Point;
}  // namespace gfx

namespace cromarc {

class SidebarView;

// Compact mode (design, UI section 9), Zen's compact mode with the sidebar
// hidden: Chrome's collapsed vertical tab strip has no width in Cromarc (hook
// H3), so collapsing leaves the page the whole window under the toolbar. The
// sidebar slides out over the page when the cursor reaches the window's left
// edge and goes 150 ms after it leaves (zen.view.compact.sidebar-keep-hover),
// and flashes for 800 ms when a tab opens in the background (Zen's
// flashSidebar).
class CompactMode : public TabStripModelObserver {
 public:
  explicit CompactMode(SidebarView* sidebar);
  CompactMode(const CompactMode&) = delete;
  CompactMode& operator=(const CompactMode&) = delete;
  ~CompactMode() override;

  // Cmd+S (Arc) and Ctrl+Alt+C (Zen), from the key monitor. False for
  // windows without a sidebar.
  static bool Toggle(BrowserWindowInterface* browser);

  bool IsCompact() const;
  // The cursor is at `screen_point` over this window.
  void OnMouseMoved(const gfx::Point& screen_point);
  void Flash();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  VerticalTabStripRegionView* Region() const;
  void Reveal();
  void HideAfter(base::TimeDelta delay);
  void Hide();

  const raw_ptr<SidebarView> sidebar_;
  std::unique_ptr<ExpandOnHoverLock> lock_;
  base::OneShotTimer hide_timer_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_COMPACT_MODE_H_
