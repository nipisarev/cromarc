// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_TABS_H_
#define CROMARC_BROWSER_PORTS_TABS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"
#include "ui/base/window_open_disposition.h"

class BrowserWindowInterface;
class GURL;

namespace blink::mojom {
class WindowFeatures;
}  // namespace blink::mojom

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
  // Closes it: it goes to the recently closed tabs.
  virtual void Close(TabId tab) = 0;
  // Closes it as the user's own gesture does.
  virtual void CloseByUser(TabId tab) = 0;
  // A copy with the page and its history, next to it. Nullopt when none was
  // made.
  virtual std::optional<TabId> Duplicate(TabId tab) = 0;

  // Opens `url` from `window` where `disposition` says, as Chrome opens a
  // URL in a window.
  virtual void Open(BrowserWindowInterface* window,
                    const GURL& url,
                    WindowOpenDisposition disposition) = 0;
  // Opens the new tab page in `window`, active.
  virtual void OpenNewTabPage(BrowserWindowInterface* window) = 0;
  // Adds the new tab page to the end of `window`, active; a hidden window
  // stays hidden when `keep_window_hidden`.
  virtual void AddNewTabPage(BrowserWindowInterface* window,
                             bool keep_window_hidden) = 0;
  // Opens `url` in `window` as a new active tab, pinned from the start, as a
  // link is followed. Nullopt when nothing opened.
  virtual std::optional<TabId> OpenPinned(BrowserWindowInterface* window,
                                          const GURL& url) = 0;
  // `contents` as a new active tab right after `window`'s active one, opened
  // from it.
  virtual std::optional<TabId> InsertAfterActive(
      BrowserWindowInterface* window,
      std::unique_ptr<content::WebContents> contents) = 0;
  // A page `source` opened (a popup, a new window), placed as Chrome places
  // the pages a tab of `window` opens. Returns it if it went into a tab.
  virtual content::WebContents* AddNewContents(
      BrowserWindowInterface* window,
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features) = 0;
  // Moves the tab, page and all, to the end of `window`, active there.
  virtual void MoveToWindow(TabId tab, BrowserWindowInterface* window) = 0;

  // Called once per insertion of tabs into `window` that leaves its active tab
  // as it was.
  virtual base::CallbackListSubscription OnOpenedInBackground(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) = 0;
  // Called when `window`'s active tab changes.
  virtual base::CallbackListSubscription OnActiveChanged(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) = 0;
  // Called as the tab is about to close for good (not to move).
  virtual base::CallbackListSubscription OnWillClose(
      TabId tab,
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_TABS_H_
