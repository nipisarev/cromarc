// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_NAVIGATION_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_NAVIGATION_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"

class BrowserWindowInterface;
class GURL;

namespace cromarc {

class Tabs;

// What Zen's address field shows for `url` in the sidebar
// (zen.urlbar.show-domain-only-in-sidebar, ZenUrlbarController.trimURL): an
// https address as its host without "www.", any other without a lone trailing
// slash; nothing for the new tab page.
std::string AddressFieldText(const GURL& url);

// The active tab's navigation in one window, for the sidebar's back, forward
// and reload buttons and its address field (Zen's single toolbar).
class SidebarNavigation : public content::WebContentsObserver {
 public:
  using Changed = base::RepeatingCallback<void(mojom::NavigationPtr)>;

  SidebarNavigation(Tabs& tabs, BrowserWindowInterface* window, Changed changed);
  SidebarNavigation(const SidebarNavigation&) = delete;
  SidebarNavigation& operator=(const SidebarNavigation&) = delete;
  ~SidebarNavigation() override;

  mojom::NavigationPtr Get();
  void Do(mojom::NavigationCommand command);

 private:
  // Follows the window's active tab.
  void Follow();
  // Tells the page once for a burst of changes.
  void Notify();
  void SendNow();

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void NavigationEntriesDeleted() override;
  void DidChangeVisibleSecurityState() override;
  void WebContentsDestroyed() override;

  const raw_ref<Tabs> tabs_;
  raw_ptr<BrowserWindowInterface> window_;
  Changed changed_;
  bool notify_pending_ = false;
  base::CallbackListSubscription active_changed_;
  base::WeakPtrFactory<SidebarNavigation> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_NAVIGATION_H_
