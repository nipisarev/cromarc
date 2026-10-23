// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom-forward.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cromarc {

class SidebarPageHandler;
class SidebarUI;

class SidebarUIConfig : public DefaultTopChromeWebUIConfig<SidebarUI> {
 public:
  SidebarUIConfig();
};

// chrome://cromarc-sidebar.top-chrome: the tab list of the browser window
// whose SidebarView hosts it.
class SidebarUI : public TopChromeWebUIController,
                  public mojom::PageHandlerFactory {
 public:
  explicit SidebarUI(content::WebUI* web_ui);
  SidebarUI(const SidebarUI&) = delete;
  SidebarUI& operator=(const SidebarUI&) = delete;
  ~SidebarUI() override;

  static constexpr std::string_view GetWebUIName() { return "CromarcSidebar"; }

  void BindInterface(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver);
  void BindInterface(
      mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<SidebarPageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> page_handler_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_UI_H_
