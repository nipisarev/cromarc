// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_OMNIBOX_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_OMNIBOX_ADAPTER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "cromarc/browser/ports/omnibox.h"

class Profile;

namespace cromarc {

// Omnibox on Chrome's AutocompleteController, with the omnibox's providers.
class OmniboxAdapter : public Omnibox, public AutocompleteController::Observer {
 public:
  explicit OmniboxAdapter(Profile* profile);
  OmniboxAdapter(const OmniboxAdapter&) = delete;
  OmniboxAdapter& operator=(const OmniboxAdapter&) = delete;
  ~OmniboxAdapter() override;

  // Omnibox:
  void Query(const std::u16string& text) override;
  void Stop() override;
  std::vector<Suggestion> Suggestions() const override;
  base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<AutocompleteController> autocomplete_;
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_OMNIBOX_ADAPTER_H_
