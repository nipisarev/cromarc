// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/theme_adapter.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/base/mojom/themes.mojom.h"

namespace cromarc {

ThemeAdapter::ThemeAdapter() = default;
ThemeAdapter::~ThemeAdapter() = default;

void ThemeAdapter::SetColor(Profile* profile, SkColor color) {
  ThemeServiceFactory::GetForProfile(profile)
      ->SetUserColorAndBrowserColorVariant(
          color, ui::mojom::BrowserColorVariant::kTonalSpot);
}

void ThemeAdapter::SetScheme(Profile* profile, Scheme scheme) {
  ThemeServiceFactory::GetForProfile(profile)->SetBrowserColorScheme(
      scheme == Scheme::kLight  ? ThemeService::BrowserColorScheme::kLight
      : scheme == Scheme::kDark ? ThemeService::BrowserColorScheme::kDark
                                : ThemeService::BrowserColorScheme::kSystem);
}

}  // namespace cromarc
