// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_UI_H_
#define CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "cromarc/browser/ui/command_bar/command_bar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cromarc {

class CommandBarPageHandler;
class CommandBarUI;

class CommandBarUIConfig : public DefaultTopChromeWebUIConfig<CommandBarUI> {
 public:
  CommandBarUIConfig();
};

// chrome://cromarc-command-bar.top-chrome, hosted by CommandBar.
class CommandBarUI : public TopChromeWebUIController,
                     public command_bar::mojom::PageHandlerFactory {
 public:
  explicit CommandBarUI(content::WebUI* web_ui);
  CommandBarUI(const CommandBarUI&) = delete;
  CommandBarUI& operator=(const CommandBarUI&) = delete;
  ~CommandBarUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "CromarcCommandBar";
  }

  void BindInterface(
      mojo::PendingReceiver<command_bar::mojom::PageHandlerFactory> receiver);

  // command_bar::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<command_bar::mojom::Page> page,
      mojo::PendingReceiver<command_bar::mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<CommandBarPageHandler> page_handler_;
  mojo::Receiver<command_bar::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_UI_H_
