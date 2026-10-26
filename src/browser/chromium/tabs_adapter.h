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

 private:
  class WindowEvents;

  WindowEvents& EventsOf(BrowserWindowInterface* window);
  void OnWindowClosed(BrowserWindowInterface* window);

  std::map<BrowserWindowInterface*, std::unique_ptr<WindowEvents>> events_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
