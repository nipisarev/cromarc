// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/hovered_link.h"

#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/ui/glance/glance.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace cromarc {

namespace {

constexpr int kModifiers =
    blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kControlKey |
    blink::WebInputEvent::kAltKey | blink::WebInputEvent::kMetaKey;

}  // namespace

// static
void HoveredLink::Set(Tabs& tabs,
                      Windows& windows,
                      content::WebContents* contents,
                      const GURL& url) {
  CreateForWebContents(contents, tabs, windows);
  HoveredLink* link = FromWebContents(contents);
  link->url_ = url;
  link->Watch(contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
}

HoveredLink::HoveredLink(content::WebContents* contents,
                         Tabs& tabs,
                         Windows& windows)
    : WebContentsUserData(*contents),
      tabs_(tabs),
      windows_(windows),
      callback_(base::BindRepeating(&HoveredLink::OnMouseEvent,
                                    base::Unretained(this))) {}

HoveredLink::~HoveredLink() {
  Watch(nullptr);
}

void HoveredLink::Watch(content::RenderWidgetHost* host) {
  if (host == host_) {
    return;
  }
  if (host_) {
    host_->RemoveMouseEventCallback(callback_);
    host_observation_.Reset();
  }
  host_ = host;
  if (host_) {
    host_->AddMouseEventCallback(callback_);
    host_observation_.Observe(host_);
  }
}

void HoveredLink::RenderWidgetHostDestroyed(content::RenderWidgetHost* host) {
  Watch(nullptr);
}

bool HoveredLink::OnMouseEvent(const blink::WebMouseEvent& event) {
  if (event.GetType() == blink::WebInputEvent::Type::kMouseUp &&
      swallow_mouse_up_) {
    swallow_mouse_up_ = false;
    return true;
  }
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown ||
      event.button != blink::WebPointerProperties::Button::kLeft ||
      (event.GetModifiers() & kModifiers) != blink::WebInputEvent::kAltKey ||
      event.click_count != 1 || !url_.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::optional<TabId> tab = tabs_->FromContents(&GetWebContents());
  BrowserWindowInterface* browser = tab ? tabs_->GetWindow(*tab) : nullptr;
  if (!browser || !windows_->IsNormal(browser)) {
    return false;
  }
  gfx::Point point = gfx::ToRoundedPoint(event.PositionInScreen());
  Glance::Open(*tabs_, *windows_, browser, url_, point);
  swallow_mouse_up_ = true;
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HoveredLink);

}  // namespace cromarc
