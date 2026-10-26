// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "cromarc/browser/ui/welcome/welcome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

class DefaultBrowser;
class Profiles;
class Tabs;
class Theme;
class Windows;

class WelcomePageHandler : public welcome::mojom::PageHandler {
 public:
  WelcomePageHandler(
      mojo::PendingReceiver<welcome::mojom::PageHandler> receiver,
      content::WebContents* web_contents,
      Profiles& profiles,
      Windows& windows,
      Tabs& tabs,
      Theme& theme,
      DefaultBrowser& default_browser);
  WelcomePageHandler(const WelcomePageHandler&) = delete;
  WelcomePageHandler& operator=(const WelcomePageHandler&) = delete;
  ~WelcomePageHandler() override;

  // welcome::mojom::PageHandler:
  void GetState(GetStateCallback callback) override;
  void SetupSpace(const std::string& name,
                  const std::string& icon,
                  uint32_t color) override;
  void AddSpace(const std::string& name, const std::string& icon) override;
  void SetAsDefaultBrowser(SetAsDefaultBrowserCallback callback) override;
  void OpenArcImport() override;
  void Finish() override;

 private:
  Profile* profile();

  mojo::Receiver<welcome::mojom::PageHandler> receiver_;
  raw_ptr<content::WebContents> web_contents_;
  const raw_ref<Profiles> profiles_;
  const raw_ref<Windows> windows_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Theme> theme_;
  const raw_ref<DefaultBrowser> default_browser_;
  base::WeakPtrFactory<WelcomePageHandler> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_
