// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_OMNIBOX_H_
#define CROMARC_BROWSER_PORTS_OMNIBOX_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace cromarc {

// The address bar's suggestions for a profile.
class Omnibox {
 public:
  struct Suggestion {
    std::u16string contents;
    std::u16string description;
    // Not always valid.
    GURL url;
  };

  virtual ~Omnibox() = default;

  // Starts the suggestions for `text`; they come in through OnChanged().
  virtual void Query(const std::u16string& text) = 0;
  virtual void Stop() = 0;
  // Best first.
  virtual std::vector<Suggestion> Suggestions() const = 0;
  virtual base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_OMNIBOX_H_
