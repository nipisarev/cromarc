// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/cromarc_resources.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "chrome/grit/tab_strip_api_resources_map.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/lifecycle/lifecycle_service.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/browser/ui/sidebar/library_downloads.h"
#include "cromarc/browser/ui/sidebar/sidebar_page_handler.h"
#include "ui/webui/webui_util.h"

namespace cromarc {

SidebarUIConfig::SidebarUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, kSidebarHost) {}

SidebarUI::SidebarUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kSidebarHost);
  webui::SetupWebUIDataSource(source, kCromarcResources,
                              IDR_CROMARC_SIDEBAR_SIDEBAR_HTML);
  // TabStripService bindings and helpers, served under /tab_strip_api/.
  source->AddResourcePaths(kTabStripApiResources);
  // Icons of pinned items that sleep.
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  // The Library's downloads: file icons, and finished pictures as previews.
  content::URLDataSource::Add(profile, std::make_unique<FileIconSource>());
  source->SetRequestFilter(
      base::BindRepeating(&LibraryDownloads::IsPreviewPath),
      base::BindRepeating(&LibraryDownloads::ServePreview,
                          base::Unretained(profile)));
}

SidebarUI::~SidebarUI() = default;

void SidebarUI::BindInterface(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver) {
  // Null when the page is opened outside a SidebarView, e.g. typed in a tab.
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return;
  }
  if (TabStripServiceFeature* const feature =
          TabStripServiceFeature::From(browser)) {
    feature->Accept(std::move(receiver));
  }
}

void SidebarUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void SidebarUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> handler) {
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  Profile* const profile = Profile::FromWebUI(web_ui());
  TreeService* const tree_service = TreeService::Get(profile);
  LifecycleService* const lifecycle_service = LifecycleService::Get(profile);
  ArchiveService* const archive_service = ArchiveService::Get(profile);
  if (!browser || !tree_service || !lifecycle_service || !archive_service) {
    return;
  }
  page_handler_ = std::make_unique<SidebarPageHandler>(
      std::move(handler), std::move(page), browser, tree_service,
      lifecycle_service, archive_service);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SidebarUI)

}  // namespace cromarc
