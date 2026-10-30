// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ui/glance/glance_registry.h"
#include "cromarc/browser/ui/glance/glance_rules.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace cromarc {

// static
void GlanceNavigationThrottle::MaybeAdd(
    content::NavigationThrottleRegistry& registry,
    Tabs& tabs,
    GlanceRegistry& glances) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() ||
      !tabs.FromContents(handle.GetWebContents())) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<GlanceNavigationThrottle>(registry, tabs, glances));
}

GlanceNavigationThrottle::GlanceNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    Tabs& tabs,
    GlanceRegistry& glances)
    : content::NavigationThrottle(registry), tabs_(tabs), glances_(glances) {}

GlanceNavigationThrottle::~GlanceNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlanceNavigationThrottle::WillStartRequest() {
  content::NavigationHandle* handle = navigation_handle();
  const std::optional<TabId> tab = tabs_->FromContents(handle->GetWebContents());
  if (!tab) {
    return PROCEED;
  }
  glances_->OnNavigationStarting(*tab, handle->GetURL());

  const ui::PageTransition transition = handle->GetPageTransition();
  LinkNavigation navigation;
  navigation.in_primary_main_frame = handle->IsInPrimaryMainFrame();
  navigation.renderer_initiated = handle->IsRendererInitiated();
  navigation.user_gesture = handle->HasUserGesture();
  navigation.same_document = handle->IsSameDocument();
  navigation.reload_or_history =
      handle->GetReloadType() != content::ReloadType::NONE ||
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
      (transition & ui::PAGE_TRANSITION_FORWARD_BACK);
  navigation.post = handle->IsPost();
  navigation.tab_kind = glances_->KindOf(*tab);
  navigation.tab_is_glance = glances_->IsGlance(*tab);
  navigation.page = handle->GetWebContents()->GetLastCommittedURL();
  navigation.url = handle->GetURL();
  if (!LeavesForGlance(navigation)) {
    return PROCEED;
  }

  // As Firefox sends such a link to a tab of its own; Glance takes it.
  content::OpenURLParams params(
      handle->GetURL(), content::Referrer(handle->GetReferrer()),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/true);
  params.initiator_origin = handle->GetInitiatorOrigin();
  params.user_gesture = true;
  // Not from inside the navigation it cancels.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Tabs* tabs, GlanceRegistry* glances, TabId tab,
                        content::OpenURLParams params) {
                       if (tabs->Exists(tab) &&
                           glances->Expect(tab, std::nullopt)) {
                         tabs->OpenFrom(tab, params);
                       }
                     },
                     // Both live as long as the browser process.
                     base::Unretained(&tabs_.get()),
                     base::Unretained(&glances_.get()), *tab,
                     std::move(params)));
  return CANCEL_AND_IGNORE;
}

const char* GlanceNavigationThrottle::GetNameForLogging() {
  return "CromarcGlanceNavigationThrottle";
}

}  // namespace cromarc
