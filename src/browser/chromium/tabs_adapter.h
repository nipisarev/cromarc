// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_

#include <map>
#include <memory>

#include "cromarc/browser/ports/tabs.h"

namespace cromarc {

// Tabs on Chrome's TabStripModel and TabInterface.
class TabsAdapter : public Tabs {
 public:
  TabsAdapter();
  TabsAdapter(const TabsAdapter&) = delete;
  TabsAdapter& operator=(const TabsAdapter&) = delete;
  ~TabsAdapter() override;

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
            bool background) override;
  void OpenNewTabPage(BrowserWindowInterface* window) override;
  std::optional<TabId> OpenPinned(BrowserWindowInterface* window,
                                  const GURL& url) override;
  base::CallbackListSubscription OnOpenedInBackground(
      BrowserWindowInterface* window,
      base::RepeatingClosure callback) override;

 private:
  class BackgroundOpenings;

  void OnWindowClosed(BrowserWindowInterface* window);

  std::map<BrowserWindowInterface*, std::unique_ptr<BackgroundOpenings>>
      background_openings_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
