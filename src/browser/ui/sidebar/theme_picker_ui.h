// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_UI_H_
#define CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "cromarc/browser/ui/sidebar/theme_picker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace cromarc {

class ThemePickerPageHandler;
class ThemePickerUI;

class ThemePickerUIConfig
    : public DefaultTopChromeWebUIConfig<ThemePickerUI> {
 public:
  ThemePickerUIConfig();
};

// chrome://cromarc-theme-picker.top-chrome, hosted by ThemePicker.
class ThemePickerUI : public TopChromeWebUIController,
                      public theme_picker::mojom::PageHandlerFactory {
 public:
  explicit ThemePickerUI(content::WebUI* web_ui);
  ThemePickerUI(const ThemePickerUI&) = delete;
  ThemePickerUI& operator=(const ThemePickerUI&) = delete;
  ~ThemePickerUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "CromarcThemePicker";
  }

  void BindInterface(
      mojo::PendingReceiver<theme_picker::mojom::PageHandlerFactory> receiver);

  // theme_picker::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<theme_picker::mojom::Page> page,
      mojo::PendingReceiver<theme_picker::mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<ThemePickerPageHandler> page_handler_;
  mojo::Receiver<theme_picker::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_UI_H_
