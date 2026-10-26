// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_FAVICONS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_FAVICONS_ADAPTER_H_

#include "cromarc/browser/ports/favicons.h"

namespace cromarc {

// Favicons on Chrome's FaviconService.
class FaviconsAdapter : public Favicons {
 public:
  FaviconsAdapter();
  FaviconsAdapter(const FaviconsAdapter&) = delete;
  FaviconsAdapter& operator=(const FaviconsAdapter&) = delete;
  ~FaviconsAdapter() override;

  // Favicons:
  void GetForPage(Profile* profile,
                  const GURL& page,
                  int size_px,
                  base::OnceCallback<void(std::string)> done,
                  base::CancelableTaskTracker* tracker) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_FAVICONS_ADAPTER_H_
