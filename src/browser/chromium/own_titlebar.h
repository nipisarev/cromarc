// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_OWN_TITLEBAR_H_
#define CROMARC_BROWSER_CHROMIUM_OWN_TITLEBAR_H_

#include <memory>

#include "base/auto_reset.h"

class BrowserView;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace cromarc {

// A popup whose title bar is Cromarc's own view (a Little window): neither
// the system's title bar nor Chrome's toolbar. The traffic lights sit centred
// in the top `height` of the window, over that view, and the page is below it
// (hooks H11, H24 and H25).

// While it lives, the popup being created gets a title bar of its own,
// `height` high.
class ScopedOwnTitlebar {
 public:
  explicit ScopedOwnTitlebar(int height);
  ScopedOwnTitlebar(const ScopedOwnTitlebar&) = delete;
  ScopedOwnTitlebar& operator=(const ScopedOwnTitlebar&) = delete;
  ~ScopedOwnTitlebar();

 private:
  base::AutoReset<int> pending_;
};

// The height of the window's own title bar; 0 for Chrome's frame.
int OwnTitlebarHeight(BrowserView* browser_view);

// Puts `view` across the top of a window that has a title bar of its own. In
// `view`, a View with views::SetHitTestComponent(HTCAPTION) moves the window;
// give each control in it HTCLIENT.
void SetOwnTitlebar(BrowserView* browser_view,
                    std::unique_ptr<views::View> view);

// Whether `point`, in the browser view's parent, is on the empty space of
// the window's own title bar.
bool IsOwnTitlebarCaption(BrowserView* browser_view, const gfx::Point& point);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_OWN_TITLEBAR_H_
