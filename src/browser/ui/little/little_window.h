// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_
#define CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

// Little Arc (design, component 5 and UI section 7): a link from another app
// opens in a small popup window of the last active space, with "Open in
// <space>" and close in its toolbar. Closing it archives the link; opening it
// in a space moves the page to that space's Today.
class LittleWindow {
 public:
  // Opens `url` in a new Little window of `profile`, centred on the screen
  // under the cursor.
  static void Open(Profile* profile, const GURL& url);
  // The Little window `browser` is, if it is one.
  static LittleWindow* From(BrowserWindowInterface* browser);

  LittleWindow(const LittleWindow&) = delete;
  LittleWindow& operator=(const LittleWindow&) = delete;
  ~LittleWindow();

  // The space the page goes to by default: the window's own.
  std::string default_space() const { return default_space_; }
  // The page's URL; empty once the window has closed.
  GURL url() const;

  // Moves the page to the space's Today and brings that space forward. A
  // space of another profile gets the URL; the page here closes unarchived.
  void OpenInSpace(const std::string& space_id);
  // Closes the window; the link goes to the archive.
  void Close();

  base::WeakPtr<LittleWindow> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  explicit LittleWindow(BrowserWindowInterface* browser);

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnSpaceShown(BrowserWindowInterface* space_window);
  void OnWindowClosed(BrowserWindowInterface* browser);

  const SessionID id_;
  const std::string default_space_;
  // Set when the page went to a space, so closing does not archive it.
  bool moved_ = false;
  base::CallbackListSubscription tab_subscription_;
  base::CallbackListSubscription close_subscription_;
  base::WeakPtrFactory<LittleWindow> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_
