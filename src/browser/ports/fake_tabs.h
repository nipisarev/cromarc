// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_TABS_H_
#define CROMARC_BROWSER_PORTS_FAKE_TABS_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/tabs.h"
#include "url/gurl.h"

namespace cromarc {

// Tabs without pages: GetContents() is always null.
class FakeTabs : public Tabs {
 public:
  FakeTabs();
  ~FakeTabs() override;

  // A new tab at the end of `window`, active if it is the window's first.
  TabId Add(BrowserWindowInterface* window, bool pinned = false);
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
  std::vector<TabId> List(BrowserWindowInterface* window) override;
  std::optional<TabId> GetActive(BrowserWindowInterface* window) override;
  bool IsPinned(TabId tab) override;
  void Activate(TabId tab) override;
  void SetPinned(TabId tab, bool pinned) override;
  void Close(TabId tab) override;
  std::optional<TabId> Duplicate(TabId tab) override;
  void Open(BrowserWindowInterface* window,
            const GURL& url,
            WindowOpenDisposition disposition) override;
  void OpenNewTabPage(BrowserWindowInterface* window) override;
  std::optional<TabId> OpenPinned(BrowserWindowInterface* window,
                                  const GURL& url) override;
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

 private:
  struct Tab {
    TabId id;
    raw_ptr<BrowserWindowInterface> window = nullptr;
    bool pinned = false;
  };

  Tab* Find(TabId tab);

  std::vector<Tab> tabs_;
  std::map<BrowserWindowInterface*, TabId> active_;
  int32_t next_id_ = 1;
  std::map<BrowserWindowInterface*, std::unique_ptr<base::RepeatingClosureList>>
      background_;
  std::map<BrowserWindowInterface*, std::unique_ptr<base::RepeatingClosureList>>
      active_changed_;
  std::map<TabId, std::unique_ptr<base::RepeatingClosureList>> will_close_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_TABS_H_
