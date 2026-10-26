// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_
#define CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/spaces/shared_sidebar_state.h"
#include "cromarc/browser/spaces/swipe_tracker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

class Profiles;
class StripDefaults;
class Tabs;
class Theme;
class VerticalStrip;

// Spaces are profiles (design, premise 1): one browser process sees all of
// them, and each space has one main window. Switching keeps one window on
// screen: at the end of the sidebar's animation the target space's window
// takes the frame of the current one, which is ordered out (component 4).
class SpaceController {
 public:
  struct Space {
    // Profile directory base name ("Default", "Profile 1").
    std::string id;
    base::FilePath path;
    std::u16string name;
    std::string icon;
    SkColor color = SK_ColorTRANSPARENT;
    // The theme editor's JSON, "" for none.
    std::string theme;
  };

  // What a window's sidebar receives from the controller.
  class Sidebar {
   public:
    virtual void OnSpacesChanged() = 0;
    // Animate to `space_id`, then call SwitchTo() (hotkeys, wheel).
    virtual void OnSwitchRequested(const std::string& space_id) = 0;
    virtual void OnSwipe(const SwipeTracker::Event& event) = 0;

   protected:
    virtual ~Sidebar() = default;
  };

  // The browser-process singleton (chromium/space_controller_get.cc).
  static SpaceController* Get();

  // Get() builds it with the Chromium ports; tests, with fakes.
  SpaceController(Profiles& profiles,
                  Windows& windows,
                  Tabs& tabs,
                  Theme& theme,
                  StripDefaults& strip_defaults);
  ~SpaceController();

  SpaceController(const SpaceController&) = delete;
  SpaceController& operator=(const SpaceController&) = delete;

  // In the user's order.
  std::vector<Space> GetSpaces();
  static std::string IdOf(Profile* profile);
  // Null when the space's profile is not loaded yet.
  Profile* GetLoadedProfile(const std::string& space_id);
  // The space's main window: its most recently active normal window.
  static BrowserWindowInterface* GetWindowFor(Profile* profile);
  // The space's main window; a hidden one, as the warm windows, when it has
  // none yet. Null when Chrome can't create it.
  BrowserWindowInterface* EnsureHiddenWindow(Profile* profile);

  void AddSidebar(BrowserWindowInterface* browser, Sidebar* sidebar);
  void RemoveSidebar(BrowserWindowInterface* browser);
  // A space window's strip shares its state with the others while the result
  // lives (SharedSidebarState).
  [[nodiscard]] base::ScopedClosureRunner ShareSidebarState(
      VerticalStrip& strip);

  // Cmd+W on the last tab of a space window: the window stays, with a new
  // tab, as in Arc. Returns false when Chrome should close the tab itself.
  bool KeepWindowOnLastTabClose(BrowserWindowInterface* browser);

  // Ask `from`'s sidebar to animate to a space (it then calls SwitchTo()).
  void RequestSwitchTo(BrowserWindowInterface* from, const std::string& id);
  void RequestSwitchToIndex(BrowserWindowInterface* from, size_t index);
  // `step` +1 next, -1 previous, round the end (Zen's changeWorkspaceShortcut).
  void RequestSwitchBy(BrowserWindowInterface* from, int step);
  // The window swap: the target space's window (created if needed) takes
  // `from`'s frame and comes forward; `from` is ordered out.
  void SwitchTo(BrowserWindowInterface* from, const std::string& id);
  // Zen switches spaces inside one window; each space here has a window of
  // its own, and a window shown from hidden paints anew. So that the swap at
  // the end of the sidebar's animation shows a window that has painted,
  // `from`'s sidebar asks for the target's window as the animation starts:
  // it comes over `from` at 1% opacity, letting the mouse through, until
  // SwitchTo() or another call ("" puts it away again).
  void PrepareSwitch(BrowserWindowInterface* from, const std::string& id);

  // Brings space `id` forward as a switch from the visible space window does
  // (loading its profile and creating its window if needed), then runs
  // `then` with that window, or null when the profile failed to load. For
  // Little windows' "Open in".
  void ShowSpace(const std::string& id,
                 base::OnceCallback<void(BrowserWindowInterface*)> then);

  void OnSwipe(BrowserWindowInterface* browser, const SwipeTracker::Event& event);

  // Runs `callback` with space `id`'s profile, loading it first if needed;
  // null when there is no such space or it failed to load.
  void LoadSpace(const std::string& id,
                 base::OnceCallback<void(Profile*)> callback);

  // Space settings (design, UI section 11). A space is a profile: these load
  // it when needed.
  // A new profile named `name` with `icon` and a colour of its own; `from`
  // switches to it.
  // `configure` runs on the new profile before the switch; without
  // `switch_to_it` the window stays where it is.
  void CreateSpace(BrowserWindowInterface* from,
                   const std::u16string& name,
                   const std::string& icon,
                   base::OnceCallback<void(Profile*)> configure = {},
                   bool switch_to_it = true);
  void RenameSpace(const std::string& id, const std::u16string& name);
  // An emoji, "icon:<name>" for one of Zen's icons, or "" for the first
  // letter of the name.
  void SetSpaceIcon(const std::string& id, const std::string& icon);
  // The theme editor's JSON. The first colour becomes the profile's Chrome
  // colour and the scheme its light or dark mode, so the window around the
  // sidebar follows.
  void SetSpaceTheme(const std::string& id, const std::string& theme);
  // The theme picker's scheme buttons ("auto", "light", "dark") act at once,
  // as Zen's do; the theme saved when the picker closes carries the scheme.
  void SetSpaceScheme(const std::string& id, const std::string& scheme);
  // `ids` in the switcher's new order.
  void ReorderSpaces(const std::vector<std::string>& ids);
  // Deletes the space's profile the way Chrome's settings do, after `from`
  // moved to another space if it shows this one. False for the last space.
  bool DeleteSpace(BrowserWindowInterface* from, const std::string& id);

  // A space window got its sidebar. Right after startup, session restore can
  // bring back the windows of every space: all but the last active one hide.
  void OnWindowCreated(BrowserWindowInterface* browser);

 private:


  void NotifySpacesChanged();
  // `profile`'s most recently active normal window.
  BrowserWindowInterface* WindowFor(Profile* profile);
  // Shown and not minimized.
  bool IsOnScreen(BrowserWindowInterface* browser);
  // Null-safe.
  bool IsFullscreen(BrowserWindowInterface* browser);
  // The frame a space window takes: a fullscreen window's is the one it
  // leaves fullscreen to.
  gfx::Rect FrameOf(BrowserWindowInterface* browser);
  // The parts of a space theme Chrome's own theme takes.
  void ApplyTheme(Profile* profile, const std::string& json);
  // A new window of `profile` starts as the shared sidebar state.
  void SeedSidebarState(Profile* profile);
  void HideInactiveSpaceWindows();
  // Warm switching: every space gets a hidden window after startup, so a
  // switch only has to swap windows (design, component 4).
  void PrepareWarmWindows();
  // Closing the visible space window closes the hidden ones of the other
  // spaces too: they are one window to the user.
  void OnSpaceWindowClosed(BrowserWindowInterface* browser);
  void SwitchToLoaded(WindowId from_id, Profile* target);
  void ShowLoadedSpace(base::OnceCallback<void(BrowserWindowInterface*)> then,
                       Profile* target);
  void OnSpaceCreated(WindowId from_id,
                      std::string icon,
                      base::OnceCallback<void(Profile*)> configure,
                      bool switch_to_it,
                      Profile* profile);
  // The space window on screen: the most recently active one with a sidebar.
  BrowserWindowInterface* GetVisibleSpaceWindow();
  // `profile`'s space window, created with `bounds` when it has none.
  BrowserWindowInterface* EnsureWindow(Profile* profile,
                                       const gfx::Rect& bounds);
  void PrepareLoaded(WindowId from_id, Profile* target);
  // Back to normal, ordered out when `hide`.
  void ClearPrepared(bool hide);
  bool IsPrepared(BrowserWindowInterface* browser) const;

  const raw_ref<Profiles> profiles_;
  const raw_ref<Windows> windows_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Theme> theme_;
  const raw_ref<StripDefaults> strip_defaults_;
  std::map<BrowserWindowInterface*, raw_ptr<Sidebar>> sidebars_;
  std::map<BrowserWindowInterface*, base::CallbackListSubscription>
      close_subscriptions_;
  SharedSidebarState sidebar_state_;
  bool warm_windows_scheduled_ = false;
  // The window PrepareSwitch() brought over the visible one, and the space it
  // was asked for while its profile loads.
  std::optional<WindowId> prepared_;
  std::string preparing_;
  base::OneShotTimer prepared_timeout_;
  const base::TimeTicks started_ = base::TimeTicks::Now();
  base::CallbackListSubscription profiles_changed_;
  base::WeakPtrFactory<SpaceController> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_
