// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/theme_picker_ui.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/cromarc_resources.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/theme_picker_page_handler.h"
#include "cromarc/browser/ui/webui_strings.h"
#include "ui/webui/webui_util.h"

namespace cromarc {

ThemePickerUIConfig::ThemePickerUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, kThemePickerHost) {
}

ThemePickerUI::ThemePickerUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kThemePickerHost);
  webui::SetupWebUIDataSource(source, kCromarcResources,
                              IDR_CROMARC_THEME_PICKER_THEME_PICKER_HTML);
  AddCromarcStrings(source, g_browser_process->GetApplicationLocale());
}

ThemePickerUI::~ThemePickerUI() = default;

void ThemePickerUI::BindInterface(
    mojo::PendingReceiver<theme_picker::mojom::PageHandlerFactory> receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void ThemePickerUI::CreatePageHandler(
    mojo::PendingRemote<theme_picker::mojom::Page> page,
    mojo::PendingReceiver<theme_picker::mojom::PageHandler> handler) {
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return;
  }
  page_handler_.reset();
  page_handler_ = std::make_unique<ThemePickerPageHandler>(
      std::move(handler), std::move(page), browser, ChromiumWindows());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ThemePickerUI)

}  // namespace cromarc
