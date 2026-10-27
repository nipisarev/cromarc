// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/welcome/welcome_page_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/ports/default_browser.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/theme.h"
#include "cromarc/browser/ports/windows.h"
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
    content::WebContents* web_contents,
    Profiles& profiles,
    Windows& windows,
    Tabs& tabs,
    Theme& theme,
    DefaultBrowser& default_browser)
    : receiver_(this, std::move(receiver)),
      web_contents_(web_contents),
      profiles_(profiles),
      windows_(windows),
      tabs_(tabs),
      theme_(theme),
      default_browser_(default_browser) {}

WelcomePageHandler::~WelcomePageHandler() = default;

Profile* WelcomePageHandler::profile() {
  return profiles_->FromBrowserContext(web_contents_->GetBrowserContext());
}

void WelcomePageHandler::GetState(GetStateCallback callback) {
  auto state = welcome::mojom::WelcomeState::New();
  const std::string id = SpaceController::IdOf(profile());
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    if (space.id == id) {
      // Chromium's own name for a new profile is no name of the user's: the
      // page suggests one instead.
      if (!profiles_->HasDefaultName(id)) {
        state->space_name = base::UTF16ToUTF8(space.name);
      }
      state->space_icon = space.icon;
    }
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ArcFound),
      // The port outlives the page: it is the browser's.
      base::BindOnce(
          [](DefaultBrowser* default_browser,
             welcome::mojom::WelcomeStatePtr state, GetStateCallback callback,
             bool arc_found) {
            state->arc_found = arc_found;
            default_browser->CheckIsDefault(base::BindOnce(
                [](welcome::mojom::WelcomeStatePtr state,
                   GetStateCallback callback, bool is_default) {
                  state->is_default_browser = is_default;
                  std::move(callback).Run(std::move(state));
                },
                std::move(state), std::move(callback)));
          },
          base::Unretained(&default_browser_.get()), std::move(state),
          std::move(callback)));
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
    theme_->SetColor(profile(), static_cast<SkColor>(color));
  }
}

void WelcomePageHandler::AddSpace(const std::string& name,
                                  const std::string& icon) {
  BrowserWindowInterface* browser = windows_->WindowOfWebUI(web_contents_);
  if (browser && !name.empty()) {
    SpaceController::Get()->CreateSpace(browser, base::UTF8ToUTF16(name), icon,
                                        {}, /*switch_to_it=*/false);
  }
}

void WelcomePageHandler::SetAsDefaultBrowser(
    SetAsDefaultBrowserCallback callback) {
  default_browser_->SetAsDefault(std::move(callback));
}

void WelcomePageHandler::OpenArcImport() {
  if (BrowserWindowInterface* browser =
          windows_->WindowOfWebUI(web_contents_)) {
    tabs_->Open(browser, GURL(kArcImportURL),
                WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

void WelcomePageHandler::Finish() {
  web_contents_->ClosePage();
}

}  // namespace cromarc
