// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_
#define CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "components/sessions/core/session_id.h"
#include "cromarc/browser/spaces/swipe_tracker.h"
#include "third_party/skia/include/core/SkColor.h"

class BrowserWindowInterface;
class Profile;
class ProfileAttributesStorage;

namespace cromarc {

// Spaces are profiles (design, premise 1): one browser process sees all of
// them, and each space has one main window. Switching keeps one window on
// screen: at the end of the sidebar's animation the target space's window
// takes the frame of the current one, which is ordered out (component 4).
class SpaceController : public ProfileAttributesStorageObserver {
 public:
  struct Space {
    // Profile directory base name ("Default", "Profile 1").
    std::string id;
    base::FilePath path;
    std::u16string name;
    std::string icon;
    SkColor color = SK_ColorTRANSPARENT;
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

  static SpaceController* Get();

  SpaceController(const SpaceController&) = delete;
  SpaceController& operator=(const SpaceController&) = delete;

  // In the user's order.
  std::vector<Space> GetSpaces();
  static std::string IdOf(Profile* profile);
  // Null when the space's profile is not loaded yet.
  Profile* GetLoadedProfile(const std::string& space_id);
  // The space's main window: its most recently active normal window.
  static BrowserWindowInterface* GetWindowFor(Profile* profile);

  void AddSidebar(BrowserWindowInterface* browser, Sidebar* sidebar);
  void RemoveSidebar(BrowserWindowInterface* browser);

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

  void OnSwipe(BrowserWindowInterface* browser, const SwipeTracker::Event& event);

  // A space window got its sidebar. Right after startup, session restore can
  // bring back the windows of every space: all but the last active one hide.
  void OnWindowCreated(BrowserWindowInterface* browser);

  // ProfileAttributesStorageObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

 private:
  friend base::NoDestructor<SpaceController>;

  SpaceController();
  ~SpaceController() override;

  void NotifySpacesChanged();
  void HideInactiveSpaceWindows();
  // Warm switching: every space gets a hidden window after startup, so a
  // switch only has to swap windows (design, component 4).
  void PrepareWarmWindows();
  void CreateHiddenWindow(Profile* profile);
  // Closing the visible space window closes the hidden ones of the other
  // spaces too: they are one window to the user.
  void OnSpaceWindowClosed(BrowserWindowInterface* browser);
  void SwitchToLoaded(SessionID from_id, Profile* target);

  std::map<BrowserWindowInterface*, raw_ptr<Sidebar>> sidebars_;
  std::map<BrowserWindowInterface*, base::CallbackListSubscription>
      close_subscriptions_;
  bool warm_windows_scheduled_ = false;
  const base::TimeTicks started_ = base::TimeTicks::Now();
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      storage_observation_{this};
  base::WeakPtrFactory<SpaceController> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SPACE_CONTROLLER_H_
