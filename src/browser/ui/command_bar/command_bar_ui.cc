// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/command_bar/command_bar_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/cromarc_resources.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/command_bar/command_bar_page_handler.h"
#include "cromarc/browser/ui/command_bar/cromarc_command_bar.h"
#include "ui/webui/webui_util.h"

namespace cromarc {

CommandBarUIConfig::CommandBarUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, kCommandBarHost) {}

CommandBarUI::CommandBarUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kCommandBarHost);
  webui::SetupWebUIDataSource(source, kCromarcResources,
                              IDR_CROMARC_COMMAND_BAR_COMMAND_BAR_HTML);
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
}

CommandBarUI::~CommandBarUI() = default;

void CommandBarUI::BindInterface(
    mojo::PendingReceiver<command_bar::mojom::PageHandlerFactory> receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void CommandBarUI::CreatePageHandler(
    mojo::PendingRemote<command_bar::mojom::Page> page,
    mojo::PendingReceiver<command_bar::mojom::PageHandler> handler) {
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  Profile* const profile = Profile::FromWebUI(web_ui());
  TreeService* const tree_service = TreeService::Get(profile);
  ArchiveService* const archive_service = ArchiveService::Get(profile);
  if (!browser || !tree_service || !archive_service) {
    return;
  }
  page_handler_.reset();
  page_handler_ = std::make_unique<CommandBarPageHandler>(
      std::move(handler), std::move(page), browser, tree_service,
      archive_service, ChromiumTabs());
}

WEB_UI_CONTROLLER_TYPE_IMPL(CommandBarUI)

}  // namespace cromarc
