// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/welcome/welcome_page_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/arc_import/arc_import_ui.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

bool ArcFound() {
  return base::PathExists(base::GetHomeDir().Append(
      "Library/Application Support/Arc/StorableSidebar.json"));
}

}  // namespace

WelcomePageHandler::WelcomePageHandler(
    mojo::PendingReceiver<welcome::mojom::PageHandler> receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)), web_contents_(web_contents) {}

WelcomePageHandler::~WelcomePageHandler() = default;

Profile* WelcomePageHandler::profile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void WelcomePageHandler::GetState(GetStateCallback callback) {
  auto state = welcome::mojom::WelcomeState::New();
  const std::string id = SpaceController::IdOf(profile());
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (space.id == id) {
      state->space_name = base::UTF16ToUTF8(space.name);
      state->space_icon = space.icon;
    }
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ArcFound),
      base::BindOnce(
          [](welcome::mojom::WelcomeStatePtr state, GetStateCallback callback,
             bool arc_found) {
            state->arc_found = arc_found;
            auto worker = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
            worker->StartCheckIsDefault(base::BindOnce(
                [](welcome::mojom::WelcomeStatePtr state,
                   GetStateCallback callback,
                   shell_integration::DefaultWebClientState is_default) {
                  state->is_default_browser =
                      is_default == shell_integration::IS_DEFAULT;
                  std::move(callback).Run(std::move(state));
                },
                std::move(state), std::move(callback)));
          },
          std::move(state), std::move(callback)));
}

void WelcomePageHandler::SetupSpace(const std::string& name,
                                    const std::string& icon,
                                    uint32_t color) {
  const std::string id = SpaceController::IdOf(profile());
  if (!name.empty()) {
    SpaceController::Get()->RenameSpace(id, base::UTF8ToUTF16(name));
  }
  SpaceController::Get()->SetSpaceIcon(id, icon);
  if (color) {
    ThemeServiceFactory::GetForProfile(profile())
        ->SetUserColorAndBrowserColorVariant(
            static_cast<SkColor>(color),
            ui::mojom::BrowserColorVariant::kTonalSpot);
  }
}

void WelcomePageHandler::AddSpace(const std::string& name,
                                  const std::string& icon) {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  if (browser && !name.empty()) {
    SpaceController::Get()->CreateSpace(browser, base::UTF8ToUTF16(name), icon,
                                        {}, /*switch_to_it=*/false);
  }
}

void WelcomePageHandler::SetAsDefaultBrowser(
    SetAsDefaultBrowserCallback callback) {
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartSetAsDefault(base::BindOnce(
          [](SetAsDefaultBrowserCallback callback,
             shell_integration::DefaultWebClientState state) {
            std::move(callback).Run(state == shell_integration::IS_DEFAULT);
          },
          std::move(callback)));
}

void WelcomePageHandler::OpenArcImport() {
  if (BrowserWindowInterface* browser =
          webui::GetBrowserWindowInterface(web_contents_)) {
    browser->OpenGURL(GURL(kArcImportURL),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

void WelcomePageHandler::Finish() {
  web_contents_->ClosePage();
}

}  // namespace cromarc
