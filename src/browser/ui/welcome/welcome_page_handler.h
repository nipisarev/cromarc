// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cromarc/browser/ui/welcome/welcome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

class WelcomePageHandler : public welcome::mojom::PageHandler {
 public:
  WelcomePageHandler(
      mojo::PendingReceiver<welcome::mojom::PageHandler> receiver,
      content::WebContents* web_contents);
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
  base::WeakPtrFactory<WelcomePageHandler> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_WELCOME_WELCOME_PAGE_HANDLER_H_
