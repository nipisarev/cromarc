// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/favicons_adapter.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "url/gurl.h"

namespace cromarc {

FaviconsAdapter::FaviconsAdapter() = default;
FaviconsAdapter::~FaviconsAdapter() = default;

void FaviconsAdapter::GetForPage(Profile* profile,
                                 const GURL& page,
                                 int size_px,
                                 base::OnceCallback<void(std::string)> done,
                                 base::CancelableTaskTracker* tracker) {
  favicon::FaviconService* favicons = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicons) {
    std::move(done).Run(std::string());
    return;
  }
  favicons->GetRawFaviconForPageURL(
      page, {favicon_base::IconType::kFavicon}, size_px,
      /*fallback_to_host=*/true,
      base::BindOnce([](const favicon_base::FaviconRawBitmapResult& result) {
        return result.is_valid() ? "data:image/png;base64," +
                                       base::Base64Encode(*result.bitmap_data)
                                 : std::string();
      }).Then(std::move(done)),
      tracker);
}

}  // namespace cromarc
