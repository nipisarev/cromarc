// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/sidebar_ui.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/cromarc_sidebar_resources.h"
#include "chrome/grit/cromarc_sidebar_resources_map.h"
#include "chrome/grit/tab_strip_api_resources_map.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "ui/webui/webui_util.h"

namespace cromarc {

SidebarUIConfig::SidebarUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, kSidebarHost) {}

SidebarUI::SidebarUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kSidebarHost);
  webui::SetupWebUIDataSource(source, kCromarcSidebarResources,
                              IDR_CROMARC_SIDEBAR_SIDEBAR_HTML);
  // TabStripService bindings and helpers, served under /tab_strip_api/.
  source->AddResourcePaths(kTabStripApiResources);
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

WEB_UI_CONTROLLER_TYPE_IMPL(SidebarUI)

}  // namespace cromarc
