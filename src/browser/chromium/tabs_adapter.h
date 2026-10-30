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
  GURL GetUrl(TabId tab) override;
  GURL GetLastCommittedUrl(TabId tab) override;
  std::u16string GetTitle(TabId tab) override;
  base::Time GetLastActiveTime(TabId tab) override;
  bool IsAudibleOrCaptured(TabId tab) override;
  SiteSecurity GetSiteSecurity(TabId tab) override;
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

 private:
  class WindowEvents;

  WindowEvents& EventsOf(BrowserWindowInterface* window);
  void OnWindowClosed(BrowserWindowInterface* window);

  std::map<BrowserWindowInterface*, std::unique_ptr<WindowEvents>> events_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_TABS_ADAPTER_H_
