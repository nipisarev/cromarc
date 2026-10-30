// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_navigation.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/ports/tabs.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace cromarc {

namespace {

mojom::SiteSecurity ToMojom(SiteSecurity security) {
  switch (security) {
    case SiteSecurity::kSecure:
      return mojom::SiteSecurity::kSecure;
    case SiteSecurity::kNotSecure:
      return mojom::SiteSecurity::kNotSecure;
    case SiteSecurity::kBrowserPage:
      return mojom::SiteSecurity::kBrowserPage;
    case SiteSecurity::kNone:
      return mojom::SiteSecurity::kNone;
  }
}

bool IsNewTabPage(const GURL& url) {
  return url.SchemeIs("chrome") &&
         (url.host() == "newtab" || url.host() == "new-tab-page");
}

}  // namespace

std::string AddressFieldText(const GURL& url) {
  if (url.is_empty() || IsNewTabPage(url)) {
    return std::string();
  }
  std::string text = url.spec();
  // BrowserUIUtils.removeSingleTrailingSlashFromURL.
  if ((url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme)) &&
      url.path() == "/" && !url.has_query() && !url.has_ref() &&
      base::EndsWith(text, "/")) {
    text.pop_back();
  }
  if (url.SchemeIs(url::kHttpsScheme)) {
    text = std::string(url.host());
    if (url.has_port()) {
      text += ":" + std::string(url.port());
    }
  }
  if (base::StartsWith(text, "www.")) {
    text = text.substr(4);
  }
  return text;
}

SidebarNavigation::SidebarNavigation(Tabs& tabs,
                                     BrowserWindowInterface* window,
                                     Changed changed)
    : tabs_(tabs), window_(window), changed_(std::move(changed)) {
  active_changed_ = tabs_->OnActiveChanged(
      window_, base::BindRepeating(&SidebarNavigation::Follow,
                                   weak_factory_.GetWeakPtr()));
  Follow();
}

SidebarNavigation::~SidebarNavigation() = default;

mojom::NavigationPtr SidebarNavigation::Get() {
  auto navigation = mojom::Navigation::New();
  navigation->security = mojom::SiteSecurity::kNone;
  const std::optional<TabId> tab = tabs_->GetActive(window_);
  content::WebContents* contents = tab ? tabs_->GetContents(*tab) : nullptr;
  if (!contents) {
    return navigation;
  }
  content::NavigationController& controller = contents->GetController();
  navigation->can_go_back = controller.CanGoBack();
  navigation->can_go_forward = controller.CanGoForward();
  navigation->loading = contents->IsLoading();
  navigation->display = AddressFieldText(contents->GetVisibleURL());
  navigation->security = ToMojom(tabs_->GetSiteSecurity(*tab));
  return navigation;
}

void SidebarNavigation::Do(mojom::NavigationCommand command) {
  content::WebContents* contents = web_contents();
  if (!contents) {
    return;
  }
  content::NavigationController& controller = contents->GetController();
  switch (command) {
    case mojom::NavigationCommand::kBack:
      if (controller.CanGoBack()) {
        controller.GoBack();
      }
      break;
    case mojom::NavigationCommand::kForward:
      if (controller.CanGoForward()) {
        controller.GoForward();
      }
      break;
    case mojom::NavigationCommand::kReload:
      controller.Reload(content::ReloadType::NORMAL, /*check_for_repost=*/true);
      break;
    case mojom::NavigationCommand::kStop:
      contents->Stop();
      break;
  }
}

void SidebarNavigation::Follow() {
  const std::optional<TabId> tab = tabs_->GetActive(window_);
  Observe(tab ? tabs_->GetContents(*tab) : nullptr);
  Notify();
}

void SidebarNavigation::Notify() {
  if (notify_pending_) {
    return;
  }
  notify_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SidebarNavigation::SendNow,
                                weak_factory_.GetWeakPtr()));
}

void SidebarNavigation::SendNow() {
  notify_pending_ = false;
  changed_.Run(Get());
}

void SidebarNavigation::DidStartLoading() {
  Notify();
}

void SidebarNavigation::DidStopLoading() {
  Notify();
}

void SidebarNavigation::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  Notify();
}

void SidebarNavigation::NavigationEntriesDeleted() {
  Notify();
}

void SidebarNavigation::DidChangeVisibleSecurityState() {
  Notify();
}

void SidebarNavigation::WebContentsDestroyed() {
  Observe(nullptr);
  Notify();
}

}  // namespace cromarc
