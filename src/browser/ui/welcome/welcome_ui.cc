// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/welcome/welcome_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/cromarc_resources.h"
#include "chrome/grit/cromarc_resources_map.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/ui/webui_strings.h"
#include "cromarc/browser/ui/welcome/welcome_page_handler.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// Beside the profiles: once per installation, whatever the spaces.
constexpr char kMarker[] = "Cromarc Welcome Shown";

// True when this call is the first to see no marker (and leaves one).
bool TakeMarker(const base::FilePath& marker) {
  if (base::PathExists(marker)) {
    return false;
  }
  return base::WriteFile(marker, "");
}

}  // namespace

WelcomeUIConfig::WelcomeUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kWelcomeHost) {}

WelcomeUI::WelcomeUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kWelcomeHost);
  webui::SetupWebUIDataSource(source, kCromarcResources,
                              IDR_CROMARC_WELCOME_WELCOME_HTML);
  AddCromarcStrings(source, g_browser_process->GetApplicationLocale());
}

WelcomeUI::~WelcomeUI() = default;

void WelcomeUI::BindInterface(
    mojo::PendingReceiver<welcome::mojom::PageHandlerFactory> receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void WelcomeUI::CreatePageHandler(
    mojo::PendingReceiver<welcome::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<WelcomePageHandler>(
      std::move(handler), web_ui()->GetWebContents(), ChromiumProfiles(),
      ChromiumWindows(), ChromiumTabs(), ChromiumTheme(),
      ChromiumDefaultBrowser());
}

WEB_UI_CONTROLLER_TYPE_IMPL(WelcomeUI)

void MaybeShowWelcome(BrowserWindowInterface* browser) {
  static bool asked = false;
  if (asked || browser->GetProfile()->IsOffTheRecord() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoFirstRun)) {
    return;
  }
  asked = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&TakeMarker,
                     g_browser_process->profile_manager()
                         ->user_data_dir()
                         .AppendASCII(kMarker)),
      base::BindOnce(
          [](SessionID id, bool first) {
            BrowserWindowInterface* browser =
                BrowserWindowInterface::FromSessionID(id);
            if (first && browser) {
              browser->OpenGURL(GURL(kWelcomeURL),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB);
            }
          },
          browser->GetSessionID()));
}

}  // namespace cromarc
