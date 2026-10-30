// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_TABS_H_
#define CROMARC_BROWSER_PORTS_FAKE_TABS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/tabs.h"
#include "url/gurl.h"

namespace cromarc {

// Tabs without pages: GetContents() is always null. A tab's page has loaded
// its URL, so the committed URL is the URL, and it was last active when it
// was added or last activated.
class FakeTabs : public Tabs {
 public:
  FakeTabs();
  ~FakeTabs() override;

  // A new tab at the end of `window`, active if it is the window's first.
  TabId Add(BrowserWindowInterface* window, bool pinned = false);
  // A new tab at the end of `window`, showing `url`, opened from `opener` and
  // active when `active`, as a page's link opens one.
  TabId AddFrom(BrowserWindowInterface* window,
                std::optional<TabId> opener,
                const GURL& url,
                bool active);
  void SetUrl(TabId tab, const GURL& url);
  // The user closed it.
  void Remove(TabId tab);
  // Takes it out of its window, as a drag between windows does.
  void Detach(TabId tab);
  void OpenInBackground(BrowserWindowInterface* window);

  // Tabs:
  bool Exists(TabId tab) override;
  content::WebContents* GetContents(TabId tab) override;
  BrowserWindowInterface* GetWindow(TabId tab) override;
  std::optional<TabId> FromContents(content::WebContents* contents) override;
  GURL GetUrl(TabId tab) override;
  GURL GetLastCommittedUrl(TabId tab) override;
  std::u16string GetTitle(TabId tab) override;
  base::Time GetLastActiveTime(TabId tab) override;
  bool IsAudibleOrCaptured(TabId tab) override;
  bool MayLoseInput(TabId tab) override;
  std::vector<TabId> List(BrowserWindowInterface* window) override;
  std::optional<TabId> GetActive(BrowserWindowInterface* window) override;
  bool IsPinned(TabId tab) override;
  void Activate(TabId tab) override;
  void SetPinned(TabId tab, bool pinned) override;
  void Close(TabId tab) override;
  void CloseByUser(TabId tab) override;
  std::optional<TabId> Duplicate(TabId tab) override;
  void Open(BrowserWindowInterface* window,
            const GURL& url,
            WindowOpenDisposition disposition) override;
  void OpenNewTabPage(BrowserWindowInterface* window) override;
  void AddNewTabPage(BrowserWindowInterface* window,
                     bool keep_window_hidden) override;
  std::optional<TabId> OpenPinned(BrowserWindowInterface* window,
                                  const GURL& url) override;
  void AddUnloaded(BrowserWindowInterface* window,
                   const GURL& url,
                   const std::u16string& title) override;
  std::optional<TabId> InsertAfterActive(
      BrowserWindowInterface* window,
      std::unique_ptr<content::WebContents> contents) override;
  content::WebContents* AddNewContents(
      BrowserWindowInterface* window,
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features) override;
  void MoveToWindow(TabId tab, BrowserWindowInterface* window) override;
  // Opens a tab right away, as Chrome would once the link loads.
  void OpenFrom(TabId tab, const content::OpenURLParams& params) override;
  void MoveToFirstUnpinned(TabId tab) override;
  base::CallbackListSubscription OnInserted(BrowserWindowInterface* window,
                                            InsertedCallback callback) override;
  base::CallbackListSubscription OnOpenedInBackground(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) override;
  base::CallbackListSubscription OnActiveChanged(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) override;
  base::CallbackListSubscription OnWillClose(
      TabId tab,
      base::RepeatingClosure callback) override;

  // What was opened, by window.
  std::vector<std::pair<BrowserWindowInterface*, GURL>> opened;
  // What OpenFrom() opened, by the tab it opened from.
  std::vector<std::pair<TabId, GURL>> opened_from;
  // The tabs closed, by Close() or CloseByUser(), in order.
  std::vector<TabId> closed;
  std::map<TabId, std::u16string> titles;
  std::set<TabId> audible;
  std::set<TabId> may_lose_input;

 private:
  struct Tab {
    TabId id;
    raw_ptr<BrowserWindowInterface> window = nullptr;
    bool pinned = false;
    GURL url;
    base::Time last_active = base::Time::Now();
  };

  void NotifyInserted(TabId tab, std::optional<TabId> opener, bool active);
  void NotifyActiveChanged(BrowserWindowInterface* window);

  Tab* Find(TabId tab);

  std::vector<Tab> tabs_;
  std::map<BrowserWindowInterface*, TabId> active_;
  int32_t next_id_ = 1;
  std::map<BrowserWindowInterface*, std::unique_ptr<base::RepeatingClosureList>>
      background_;
  std::map<BrowserWindowInterface*, std::unique_ptr<base::RepeatingClosureList>>
      active_changed_;
  std::map<TabId, std::unique_ptr<base::RepeatingClosureList>> will_close_;
  std::map<BrowserWindowInterface*,
           std::unique_ptr<base::RepeatingCallbackList<
               void(TabId, std::optional<TabId>, bool)>>>
      inserted_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_TABS_H_
