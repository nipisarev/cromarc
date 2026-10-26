// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_

#include "cromarc/browser/ports/windows.h"

namespace cromarc {

// Windows on Chrome's BrowserWindowInterface.
class WindowsAdapter : public Windows {
 public:
  WindowsAdapter();
  WindowsAdapter(const WindowsAdapter&) = delete;
  WindowsAdapter& operator=(const WindowsAdapter&) = delete;
  ~WindowsAdapter() override;

  // Windows:
  Profile* ProfileOf(BrowserWindowInterface* window) override;
  content::BrowserContext* BrowserContextOf(
      BrowserWindowInterface* window) override;
  bool IsOffTheRecord(BrowserWindowInterface* window) override;
  bool IsNormal(BrowserWindowInterface* window) override;
  WindowId IdOf(BrowserWindowInterface* window) override;
  BrowserWindowInterface* FromId(WindowId id) override;
  std::vector<BrowserWindowInterface*> NormalWindowsOf(
      Profile* profile) override;
  bool IsOnScreen(BrowserWindowInterface* window) override;
  gfx::Rect BoundsOf(BrowserWindowInterface* window) override;
  gfx::NativeWindow NativeWindowOf(BrowserWindowInterface* window) override;
  void Activate(BrowserWindowInterface* window) override;
  void Close(BrowserWindowInterface* window) override;
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
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_WINDOWS_ADAPTER_H_
