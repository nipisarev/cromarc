// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_UI_H_
#define CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_UI_H_

#include <memory>

#include "content/public/browser/webui_config.h"
#include "cromarc/browser/ui/arc_import/arc_import.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace cromarc {

inline constexpr char kArcImportHost[] = "cromarc-import";
inline constexpr char kArcImportURL[] = "chrome://cromarc-import/";

class ArcImportPageHandler;
class ArcImportUI;

class ArcImportUIConfig : public content::DefaultWebUIConfig<ArcImportUI> {
 public:
  ArcImportUIConfig();
};

// chrome://cromarc-import, a tab.
class ArcImportUI : public ui::MojoWebUIController,
                    public arc_import::mojom::PageHandlerFactory {
 public:
  explicit ArcImportUI(content::WebUI* web_ui);
  ArcImportUI(const ArcImportUI&) = delete;
  ArcImportUI& operator=(const ArcImportUI&) = delete;
  ~ArcImportUI() override;

  void BindInterface(
      mojo::PendingReceiver<arc_import::mojom::PageHandlerFactory> receiver);

  // arc_import::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<arc_import::mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<ArcImportPageHandler> page_handler_;
  mojo::Receiver<arc_import::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_UI_H_
