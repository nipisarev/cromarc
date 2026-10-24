// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_WELCOME_WELCOME_UI_H_
#define CROMARC_BROWSER_UI_WELCOME_WELCOME_UI_H_

#include <memory>

#include "content/public/browser/webui_config.h"
#include "cromarc/browser/ui/welcome/welcome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class BrowserWindowInterface;

namespace cromarc {

inline constexpr char kWelcomeHost[] = "cromarc-welcome";
inline constexpr char kWelcomeURL[] = "chrome://cromarc-welcome/";

class WelcomePageHandler;
class WelcomeUI;

class WelcomeUIConfig : public content::DefaultWebUIConfig<WelcomeUI> {
 public:
  WelcomeUIConfig();
};

// chrome://cromarc-welcome, Cromarc's first run (design, UI section 12).
class WelcomeUI : public ui::MojoWebUIController,
                  public welcome::mojom::PageHandlerFactory {
 public:
  explicit WelcomeUI(content::WebUI* web_ui);
  WelcomeUI(const WelcomeUI&) = delete;
  WelcomeUI& operator=(const WelcomeUI&) = delete;
  ~WelcomeUI() override;

  void BindInterface(
      mojo::PendingReceiver<welcome::mojom::PageHandlerFactory> receiver);

  // welcome::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<welcome::mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<WelcomePageHandler> page_handler_;
  mojo::Receiver<welcome::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Once per installation, in the first space window: the welcome tab. Not
// with --no-first-run.
void MaybeShowWelcome(BrowserWindowInterface* browser);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_WELCOME_WELCOME_UI_H_
