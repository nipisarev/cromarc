// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/arc_import/arc_import_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/cromarc_resources.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/ui/arc_import/arc_import_page_handler.h"
#include "ui/webui/webui_util.h"

namespace cromarc {

ArcImportUIConfig::ArcImportUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kArcImportHost) {}

ArcImportUI::ArcImportUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kArcImportHost);
  webui::SetupWebUIDataSource(source, kCromarcResources,
                              IDR_CROMARC_ARC_IMPORT_ARC_IMPORT_HTML);
}

ArcImportUI::~ArcImportUI() = default;

void ArcImportUI::BindInterface(
    mojo::PendingReceiver<arc_import::mojom::PageHandlerFactory> receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void ArcImportUI::CreatePageHandler(
    mojo::PendingReceiver<arc_import::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<ArcImportPageHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ArcImportUI)

}  // namespace cromarc
