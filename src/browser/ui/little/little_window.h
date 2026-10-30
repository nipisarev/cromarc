// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_
#define CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cromarc/browser/archive/archive.h"
#include "cromarc/browser/lifecycle/policy.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/windows.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}  // namespace views

namespace cromarc {

class AppLifecycle;
class Presence;
class Profiles;

// Little Arc (design, component 5 and UI section 7): a link from another app
// opens in a small popup window of the last active space, with its space,
// "Open in <space>" and close in its toolbar. Closing it archives the link;
// opening it in a space moves the page to that space's Today. Left unused for
// 6 h it archives itself, as Little Arc windows do.
class LittleWindow {
 public:
  // The rest of Cromarc, for a Little window: its bar, the spaces and the
  // archive (LittleSpaces).
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual std::unique_ptr<views::View> MakeBar(LittleWindow& window) = 0;
    // As SpaceController's.
    virtual void ShowSpace(
        const std::string& space_id,
        base::OnceCallback<void(BrowserWindowInterface*)> then) = 0;
    virtual void LoadSpace(const std::string& space_id,
                           base::OnceCallback<void(Profile*)> then) = 0;
    virtual void Archive(Profile* profile,
                         ArchiveEntry entry,
                         bool session_ending) = 0;
  };

  // What Little windows work through; all of it outlives every window.
  struct Deps {
    raw_ref<Tabs> tabs;
    raw_ref<Windows> windows;
    raw_ref<Profiles> profiles;
    raw_ref<AppLifecycle> app_lifecycle;
    raw_ref<Presence> presence;
    raw_ref<Delegate> delegate;
  };

  // Opens `url` in a new Little window of `profile`: at `bounds`, else
  // centred on the screen under the cursor.
  static void Open(const Deps& deps,
                   Profile* profile,
                   const GURL& url,
                   std::optional<gfx::Rect> bounds = std::nullopt);
  // The Little window `browser` is, if it is one.
  static LittleWindow* From(Windows& windows, BrowserWindowInterface* browser);

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
  // The page opens again in a Little window of that space, in this one's
  // place, with that space's cookies and logins; this one closes unarchived.
  void MoveToSpace(const std::string& space_id);

  base::WeakPtr<LittleWindow> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  LittleWindow(const Deps& deps, BrowserWindowInterface* browser);

  // The page's tab, while the window lives.
  std::optional<TabId> PageTab() const;
  void OnTabWillClose(TabId tab);
  // At SIGTERM or logout the process exits with its tabs open; the page goes
  // to the archive then.
  void OnClosingAllWindows(bool closing);
  void OnSpaceShown(BrowserWindowInterface* space_window);
  void Archive(TabId tab, bool session_ending);
  // Once a while: in use while it is the window in front with the user at
  // the Mac; unused for `limits_.little_idle`, it archives itself.
  void CheckIdle();
  void OnWindowClosed(BrowserWindowInterface* browser);

  const Deps deps_;
  const WindowId id_;
  const raw_ptr<Profile> profile_;
  const std::string default_space_;
  // Set when the page went to a space, so closing does not archive it.
  bool moved_ = false;
  const Limits limits_;
  base::Time last_used_;
  base::RepeatingTimer idle_timer_;
  base::CallbackListSubscription tab_subscription_;
  base::CallbackListSubscription close_subscription_;
  base::CallbackListSubscription closing_all_subscription_;
  base::WeakPtrFactory<LittleWindow> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_LITTLE_LITTLE_WINDOW_H_
