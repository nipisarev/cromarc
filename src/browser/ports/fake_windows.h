// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_
#define CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "url/gurl.h"

#include "cromarc/browser/ports/windows.h"

namespace cromarc {

// Windows and profiles here are handles that point at nothing; a test makes
// them with Handle<BrowserWindowInterface>(n) and Handle<Profile>(n).
class FakeWindows : public Windows {
 public:
  FakeWindows();
  ~FakeWindows() override;

  template <typename T>
  static T* Handle(int n) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(0x1000 * n));
  }

  // Windows:
  Profile* ProfileOf(BrowserWindowInterface* window) override;
  content::BrowserContext* BrowserContextOf(
      BrowserWindowInterface* window) override;
  bool IsOffTheRecord(BrowserWindowInterface* window) override;
  bool IsNormal(BrowserWindowInterface* window) override;
  WindowId IdOf(BrowserWindowInterface* window) override;
  BrowserWindowInterface* FromId(WindowId id) override;
  std::vector<BrowserWindowInterface*> All() override;
  std::vector<BrowserWindowInterface*> AllByActivation() override;
  std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) override;
  bool IsOnScreen(BrowserWindowInterface* window) override;
  bool IsVisible(BrowserWindowInterface* window) override;
  bool IsFullscreen(BrowserWindowInterface* window) override;
  gfx::Rect BoundsOf(BrowserWindowInterface* window) override;
  gfx::Rect RestoredBoundsOf(BrowserWindowInterface* window) override;
  void SetBounds(BrowserWindowInterface* window,
                 const gfx::Rect& bounds) override;
  gfx::NativeWindow NativeWindowOf(BrowserWindowInterface* window) override;
  void Show(BrowserWindowInterface* window) override;
  void ShowInactive(BrowserWindowInterface* window) override;
  void Hide(BrowserWindowInterface* window) override;
  void Activate(BrowserWindowInterface* window) override;
  void ToggleFullscreen(BrowserWindowInterface* window) override;
  void Close(BrowserWindowInterface* window) override;
  BrowserWindowInterface* Create(Profile* profile,
                                 const gfx::Rect& bounds,
                                 bool from_user_gesture) override;
  base::CallbackListSubscription OnClosed(
      BrowserWindowInterface* window,
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) override;
  BrowserWindowInterface* OpenPopup(Profile* profile,
                                    const gfx::Rect& bounds,
                                    const GURL& url) override;
  void AttachWebUI(BrowserWindowInterface* window,
                   content::WebContents* contents) override;
  void AddToToolbar(BrowserWindowInterface* window,
                    std::unique_ptr<views::View> view) override;

  // The window is gone: FromId() no longer finds it, OnClosed() is called.
  void UserCloses(BrowserWindowInterface* window);

  std::map<BrowserWindowInterface*, Profile*> profiles;
  std::map<Profile*, std::vector<BrowserWindowInterface*>> normal_windows;
  // Every window, last activated first.
  std::vector<BrowserWindowInterface*> windows;
  std::set<BrowserWindowInterface*> off_screen;
  std::set<BrowserWindowInterface*> fullscreen;
  std::set<BrowserWindowInterface*> not_normal;
  std::set<BrowserWindowInterface*> off_the_record;
  std::map<BrowserWindowInterface*, gfx::Rect> bounds;
  std::vector<BrowserWindowInterface*> activated;
  std::vector<BrowserWindowInterface*> closed;
  // OpenPopup() gives these, in turn, and records what it opened.
  std::vector<BrowserWindowInterface*> popups;
  std::vector<GURL> popup_urls;
  std::map<BrowserWindowInterface*, int> toolbar_views;

 private:
  std::map<BrowserWindowInterface*,
           std::unique_ptr<base::RepeatingCallbackList<void(
               BrowserWindowInterface*)>>>
      on_closed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_WINDOWS_H_
