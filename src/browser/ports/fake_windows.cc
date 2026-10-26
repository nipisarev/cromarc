// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_windows.h"

#include <algorithm>
#include <utility>

#include "ui/views/view.h"

namespace cromarc {

FakeWindows::FakeWindows() = default;
FakeWindows::~FakeWindows() = default;

Profile* FakeWindows::ProfileOf(BrowserWindowInterface* window) {
  auto it = profiles.find(window);
  return it == profiles.end() ? nullptr : it->second;
}

content::BrowserContext* FakeWindows::BrowserContextOf(
    BrowserWindowInterface* window) {
  return nullptr;
}

bool FakeWindows::IsOffTheRecord(BrowserWindowInterface* window) {
  return off_the_record.contains(window);
}

bool FakeWindows::IsNormal(BrowserWindowInterface* window) {
  return !not_normal.contains(window);
}

WindowId FakeWindows::IdOf(BrowserWindowInterface* window) {
  return WindowId(static_cast<int32_t>(reinterpret_cast<uintptr_t>(window)));
}

BrowserWindowInterface* FakeWindows::FromId(WindowId id) {
  BrowserWindowInterface* window = reinterpret_cast<BrowserWindowInterface*>(
      static_cast<uintptr_t>(id.value()));
  return std::ranges::contains(closed, window) ? nullptr : window;
}

std::vector<BrowserWindowInterface*> FakeWindows::All() {
  return windows;
}

std::vector<BrowserWindowInterface*> FakeWindows::AllByActivation() {
  return windows;
}

std::vector<BrowserWindowInterface*> FakeWindows::NormalWindowsOf(
    Profile* profile) {
  auto it = normal_windows.find(profile);
  return it == normal_windows.end() ? std::vector<BrowserWindowInterface*>()
                                    : it->second;
}

bool FakeWindows::IsOnScreen(BrowserWindowInterface* window) {
  return !off_screen.contains(window);
}

bool FakeWindows::IsVisible(BrowserWindowInterface* window) {
  return !off_screen.contains(window);
}

bool FakeWindows::IsFullscreen(BrowserWindowInterface* window) {
  return fullscreen.contains(window);
}

gfx::Rect FakeWindows::BoundsOf(BrowserWindowInterface* window) {
  auto it = bounds.find(window);
  return it == bounds.end() ? gfx::Rect(0, 0, 1100, 800) : it->second;
}

gfx::Rect FakeWindows::RestoredBoundsOf(BrowserWindowInterface* window) {
  return BoundsOf(window);
}

void FakeWindows::SetBounds(BrowserWindowInterface* window,
                            const gfx::Rect& new_bounds) {
  bounds[window] = new_bounds;
}

gfx::NativeWindow FakeWindows::NativeWindowOf(BrowserWindowInterface* window) {
  return gfx::NativeWindow();
}

void FakeWindows::Show(BrowserWindowInterface* window) {
  off_screen.erase(window);
}

void FakeWindows::ShowInactive(BrowserWindowInterface* window) {
  off_screen.erase(window);
}

void FakeWindows::Hide(BrowserWindowInterface* window) {
  off_screen.insert(window);
}

void FakeWindows::Activate(BrowserWindowInterface* window) {
  activated.push_back(window);
}

void FakeWindows::ToggleFullscreen(BrowserWindowInterface* window) {
  if (!fullscreen.erase(window)) {
    fullscreen.insert(window);
  }
}

BrowserWindowInterface* FakeWindows::Create(Profile* profile,
                                            const gfx::Rect& new_bounds,
                                            bool from_user_gesture) {
  if (popups.empty()) {
    return nullptr;
  }
  BrowserWindowInterface* window = popups.front();
  popups.erase(popups.begin());
  profiles[window] = profile;
  bounds[window] = new_bounds;
  // Not shown yet.
  off_screen.insert(window);
  windows.push_back(window);
  normal_windows[profile].push_back(window);
  return window;
}

void FakeWindows::Close(BrowserWindowInterface* window) {
  UserCloses(window);
}

void FakeWindows::UserCloses(BrowserWindowInterface* window) {
  closed.push_back(window);
  if (auto it = on_closed_.find(window); it != on_closed_.end()) {
    it->second->Notify(window);
  }
}

base::CallbackListSubscription FakeWindows::OnClosed(
    BrowserWindowInterface* window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
  auto& list = on_closed_[window];
  if (!list) {
    list = std::make_unique<
        base::RepeatingCallbackList<void(BrowserWindowInterface*)>>();
  }
  return list->Add(std::move(callback));
}

BrowserWindowInterface* FakeWindows::OpenPopup(Profile* profile,
                                               const gfx::Rect& popup_bounds,
                                               const GURL& url) {
  if (popups.empty()) {
    return nullptr;
  }
  BrowserWindowInterface* window = popups.front();
  popups.erase(popups.begin());
  popup_urls.push_back(url);
  profiles[window] = profile;
  return window;
}

void FakeWindows::AttachWebUI(BrowserWindowInterface* window,
                              content::WebContents* contents) {}

void FakeWindows::AddToToolbar(BrowserWindowInterface* window,
                               std::unique_ptr<views::View> view) {
  ++toolbar_views[window];
}

}  // namespace cromarc
