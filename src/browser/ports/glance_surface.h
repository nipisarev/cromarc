// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_GLANCE_SURFACE_H_
#define CROMARC_BROWSER_PORTS_GLANCE_SURFACE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "cromarc/browser/ports/tabs.h"
#include "ui/gfx/geometry/point.h"

class BrowserWindowInterface;

namespace cromarc {

// How a window shows a glance (Zen's glance overlay): its active tab's page as
// a card over a picture of the tab the glance was opened from, dimmed, with
// the glance's buttons beside the card.
class GlanceSurface {
 public:
  // What the buttons, a click beside the card and Esc ask for.
  class Client {
   public:
    virtual void CloseGlance(BrowserWindowInterface* window) = 0;
    virtual void ExpandGlance(BrowserWindowInterface* window) = 0;
    // Esc in the glance's page, its top frame; `in_field` while a text field
    // there has the focus.
    virtual void EscapeGlance(BrowserWindowInterface* window,
                              bool in_field) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~GlanceSurface() = default;

  // Keeps a picture of `tab`'s page for the glances shown over it; taken
  // while the page shows, before a glance covers it.
  virtual void Snapshot(TabId tab) = 0;
  virtual void Forget(TabId tab) = 0;
  // `glance`, the active tab of `window`, shows as a card over `parent`'s
  // picture. It grows out of `origin` (screen coordinates) when there is one,
  // else it is there at once.
  virtual void Show(BrowserWindowInterface* window,
                    TabId glance,
                    TabId parent,
                    std::optional<gfx::Point> origin,
                    Client* client) = 0;
  // `window` shows its page plainly again: at once, or after the card shrank
  // away when `animate`. Then `done` runs.
  virtual void Hide(BrowserWindowInterface* window,
                    bool animate,
                    base::OnceClosure done) = 0;
  // The close button asks for a confirmation for `duration`, as Zen's
  // waitconfirmation: red, with "Confirm" beside its icon.
  virtual void AskToConfirmClose(BrowserWindowInterface* window,
                                 base::TimeDelta duration) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_GLANCE_SURFACE_H_
