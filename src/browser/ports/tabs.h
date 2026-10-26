// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_TABS_H_
#define CROMARC_BROWSER_PORTS_TABS_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"

class BrowserWindowInterface;
class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

// A tab for as long as it lives, through discards and moves between windows.
using TabId = base::StrongAlias<class TabIdTag, int32_t>;

// The tabs of the browser's windows. Windows are BrowserWindowInterface
// pointers, used as handles only.
class Tabs {
 public:
  virtual ~Tabs() = default;

  // The tab is still there, in a window or on its way between two.
  virtual bool Exists(TabId tab) = 0;
  // Null once the tab is gone.
  virtual content::WebContents* GetContents(TabId tab) = 0;
  // Null once the tab is gone, and between windows.
  virtual BrowserWindowInterface* GetWindow(TabId tab) = 0;
  // The tab showing `contents`, if it is one.
  virtual std::optional<TabId> FromContents(content::WebContents* contents) = 0;

  // `window`'s tabs, in their order.
  virtual std::vector<TabId> List(BrowserWindowInterface* window) = 0;
  virtual std::optional<TabId> GetActive(BrowserWindowInterface* window) = 0;
  virtual bool IsPinned(TabId tab) = 0;

  // Activates the tab in its window.
  virtual void Activate(TabId tab) = 0;
  virtual void SetPinned(TabId tab, bool pinned) = 0;
  // Closes it, as the user would: it goes to the recently closed tabs.
  virtual void Close(TabId tab) = 0;
  // A copy with the page and its history, next to it. Nullopt when none was
  // made.
  virtual std::optional<TabId> Duplicate(TabId tab) = 0;

  // Opens `url` in `window` as a new tab, active unless `background`.
  virtual void Open(BrowserWindowInterface* window,
                    const GURL& url,
                    bool background) = 0;
  // Opens the new tab page in `window`, active.
  virtual void OpenNewTabPage(BrowserWindowInterface* window) = 0;
  // Opens `url` in `window` as a new active tab, pinned from the start, as a
  // link is followed. Nullopt when nothing opened.
  virtual std::optional<TabId> OpenPinned(BrowserWindowInterface* window,
                                          const GURL& url) = 0;

  // Called once per insertion of tabs into `window` that leaves its active tab
  // as it was.
  virtual base::CallbackListSubscription OnOpenedInBackground(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_TABS_H_
