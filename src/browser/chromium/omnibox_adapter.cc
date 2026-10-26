// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/omnibox_adapter.h"

#include <utility>

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace cromarc {

OmniboxAdapter::OmniboxAdapter(Profile* profile)
    : profile_(profile),
      autocomplete_(std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          AutocompleteControllerConfig{
              .provider_types =
                  AutocompleteClassifier::DefaultOmniboxProviders()})) {
  autocomplete_->AddObserver(this);
}

OmniboxAdapter::~OmniboxAdapter() {
  autocomplete_->RemoveObserver(this);
}

void OmniboxAdapter::Query(const std::u16string& text) {
  autocomplete_->Start(AutocompleteInput(
      text, metrics::OmniboxEventProto::OTHER,
      ChromeAutocompleteSchemeClassifier(profile_)));
}

void OmniboxAdapter::Stop() {
  autocomplete_->Stop(AutocompleteStopReason::kClobbered);
}

std::vector<Omnibox::Suggestion> OmniboxAdapter::Suggestions() const {
  std::vector<Suggestion> suggestions;
  const AutocompleteResult& matches = autocomplete_->result();
  for (size_t i = 0; i < matches.size(); ++i) {
    const AutocompleteMatch& match = matches.match_at(i);
    suggestions.push_back(
        {match.contents, match.description, match.destination_url});
  }
  return suggestions;
}

base::CallbackListSubscription OmniboxAdapter::OnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void OmniboxAdapter::OnResultChanged(AutocompleteController* controller,
                                     bool default_match_changed) {
  on_changed_.Notify();
}

}  // namespace cromarc
