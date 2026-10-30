// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_NAVIGATION_THROTTLE_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace cromarc {

class GlanceRegistry;
class Tabs;

// Firefox's app tabs, which Zen's Glance builds on: a link the user follows on
// an Essential's or a pinned tab's page to another site does not take the
// tab away from its page; it opens in a glance over it. Also tells the
// registry about every navigation that starts in a tab (window.open).
class GlanceNavigationThrottle : public content::NavigationThrottle {
 public:
  // For hook H22: one for each navigation of a tab's page.
  static void MaybeAdd(content::NavigationThrottleRegistry& registry,
                       Tabs& tabs,
                       GlanceRegistry& glances);

  GlanceNavigationThrottle(content::NavigationThrottleRegistry& registry,
                           Tabs& tabs,
                           GlanceRegistry& glances);
  ~GlanceNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  const raw_ref<Tabs> tabs_;
  const raw_ref<GlanceRegistry> glances_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_NAVIGATION_THROTTLE_H_
