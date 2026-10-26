// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_WINDOWS_H_
#define CROMARC_BROWSER_PORTS_WINDOWS_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace cromarc {

// A window for as long as it lives; unlike its handle, never another window's
// once it is gone.
using WindowId = base::StrongAlias<class WindowIdTag, int32_t>;

// The browser's windows. Windows (BrowserWindowInterface) and profiles are
// pointers used as handles only.
class Windows {
 public:
  virtual ~Windows() = default;

  virtual Profile* ProfileOf(BrowserWindowInterface* window) = 0;
  virtual content::BrowserContext* BrowserContextOf(
      BrowserWindowInterface* window) = 0;
  virtual bool IsOffTheRecord(BrowserWindowInterface* window) = 0;
  // A normal window: tabs and a sidebar, not a popup or an app.
  virtual bool IsNormal(BrowserWindowInterface* window) = 0;
  virtual WindowId IdOf(BrowserWindowInterface* window) = 0;
  // Null once that window is gone.
  virtual BrowserWindowInterface* FromId(WindowId id) = 0;
  // `profile`'s normal windows, the ones with a sidebar.
  virtual std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) = 0;
  // Shown and not minimized.
  virtual bool IsOnScreen(BrowserWindowInterface* window) = 0;
  virtual gfx::Rect BoundsOf(BrowserWindowInterface* window) = 0;
  virtual gfx::NativeWindow NativeWindowOf(BrowserWindowInterface* window) = 0;
  virtual void Activate(BrowserWindowInterface* window) = 0;
  virtual void Close(BrowserWindowInterface* window) = 0;
  virtual base::CallbackListSubscription OnClosed(
      BrowserWindowInterface* window,
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) = 0;

  // Opens `url` in a new popup window of `profile` at `bounds`, shown, and
  // left out of session restore. Null when no window took it.
  virtual BrowserWindowInterface* OpenPopup(Profile* profile,
                                            const gfx::Rect& bounds,
                                            const GURL& url) = 0;
  // Gives `contents`, a WebUI of the window's own chrome (a panel over it),
  // its window, as Chrome's top chrome WebUIs have it.
  virtual void AttachWebUI(BrowserWindowInterface* window,
                           content::WebContents* contents) = 0;
  // Adds `view` at the end of the window's toolbar.
  virtual void AddToToolbar(BrowserWindowInterface* window,
                            std::unique_ptr<views::View> view) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_WINDOWS_H_
