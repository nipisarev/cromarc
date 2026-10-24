// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/webui_registration.h"

#include <memory>

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/webui_config_map.h"
#include "cromarc/browser/ui/arc_import/arc_import.mojom.h"
#include "cromarc/browser/ui/arc_import/arc_import_ui.h"
#include "cromarc/browser/ui/command_bar/command_bar.mojom.h"
#include "cromarc/browser/ui/command_bar/command_bar_ui.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"
#include "cromarc/browser/ui/sidebar/sidebar_ui.h"
#include "cromarc/browser/ui/welcome/welcome.mojom.h"
#include "cromarc/browser/ui/welcome/welcome_ui.h"

namespace cromarc {

void RegisterWebUIConfigs(content::WebUIConfigMap& map) {
  map.AddWebUIConfig(std::make_unique<ArcImportUIConfig>());
  map.AddWebUIConfig(std::make_unique<CommandBarUIConfig>());
  map.AddWebUIConfig(std::make_unique<SidebarUIConfig>());
  map.AddWebUIConfig(std::make_unique<WelcomeUIConfig>());
}

void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  registry.ForWebUI<ArcImportUI>()
      .Add<arc_import::mojom::PageHandlerFactory>();
  registry.ForWebUI<CommandBarUI>()
      .Add<command_bar::mojom::PageHandlerFactory>();
  registry.ForWebUI<SidebarUI>()
      .Add<tabs_api::mojom::TabStripService>()
      .Add<mojom::PageHandlerFactory>();
  registry.ForWebUI<WelcomeUI>().Add<welcome::mojom::PageHandlerFactory>();
}

}  // namespace cromarc
