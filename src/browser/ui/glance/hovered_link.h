// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_HOVERED_LINK_H_
#define CROMARC_BROWSER_UI_GLANCE_HOVERED_LINK_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace cromarc {

// The link under the cursor in a tab, as the status bubble shows it (hook
// H10). While there is one, Option+click (Zen's default Glance modifier,
// zen.glance.activation-method "alt") opens it in Glance instead of Chrome's
// download.
class HoveredLink : public content::WebContentsUserData<HoveredLink>,
                    public content::RenderWidgetHostObserver {
 public:
  static void Set(content::WebContents* contents, const GURL& url);

  ~HoveredLink() override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override;

 private:
  friend WebContentsUserData;

  explicit HoveredLink(content::WebContents* contents);

  // Follows the main frame's widget, which a cross-site navigation replaces.
  void Watch(content::RenderWidgetHost* host);
  bool OnMouseEvent(const blink::WebMouseEvent& event);

  GURL url_;
  bool swallow_mouse_up_ = false;
  raw_ptr<content::RenderWidgetHost> host_ = nullptr;
  const content::RenderWidgetHost::MouseEventCallback callback_;
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      host_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_HOVERED_LINK_H_
