// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_

#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace cromarc {

class SidebarUI;

// Hook H4 registers this in chrome/browser/ui/webui/chrome_web_ui_configs.cc.
class SidebarUIConfig : public DefaultTopChromeWebUIConfig<SidebarUI> {
 public:
  SidebarUIConfig();
};

// chrome://cromarc-sidebar.top-chrome: the tab list of the browser window
// whose SidebarView hosts it.
class SidebarUI : public TopChromeWebUIController {
 public:
  explicit SidebarUI(content::WebUI* web_ui);
  SidebarUI(const SidebarUI&) = delete;
  SidebarUI& operator=(const SidebarUI&) = delete;
  ~SidebarUI() override;

  static constexpr std::string_view GetWebUIName() { return "CromarcSidebar"; }

  // Hook H4 adds this binder in
  // chrome/browser/chrome_browser_interface_binders_webui_parts_desktop.cc.
  void BindInterface(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_
