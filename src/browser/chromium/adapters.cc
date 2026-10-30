// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/adapters.h"

#include "base/no_destructor.h"
#include "cromarc/browser/chromium/app_lifecycle_adapter.h"
#include "cromarc/browser/chromium/default_browser_adapter.h"
#include "cromarc/browser/chromium/favicons_adapter.h"
#include "cromarc/browser/chromium/last_session_adapter.h"
#include "cromarc/browser/chromium/presence_adapter.h"
#include "cromarc/browser/chromium/profiles_adapter.h"
#include "cromarc/browser/chromium/strip_defaults_adapter.h"
#include "cromarc/browser/chromium/tab_strip_api_adapter.h"
#include "cromarc/browser/chromium/tabs_adapter.h"
#include "cromarc/browser/chromium/theme_adapter.h"
#include "cromarc/browser/chromium/windows_adapter.h"

namespace cromarc {

Tabs& ChromiumTabs() {
  static base::NoDestructor<TabsAdapter> tabs;
  return *tabs;
}

Windows& ChromiumWindows() {
  static base::NoDestructor<WindowsAdapter> windows;
  return *windows;
}

AppLifecycle& ChromiumAppLifecycle() {
  static base::NoDestructor<AppLifecycleAdapter> app_lifecycle;
  return *app_lifecycle;
}

LastSession& ChromiumLastSession() {
  static base::NoDestructor<LastSessionAdapter> last_session;
  return *last_session;
}

Presence& ChromiumPresence() {
  static base::NoDestructor<PresenceAdapter> presence;
  return *presence;
}

TabStripApi& ChromiumTabStripApi() {
  static base::NoDestructor<TabStripApiAdapter> tab_strip_api;
  return *tab_strip_api;
}

Profiles& ChromiumProfiles() {
  static base::NoDestructor<ProfilesAdapter> profiles;
  return *profiles;
}

Theme& ChromiumTheme() {
  static base::NoDestructor<ThemeAdapter> theme;
  return *theme;
}

DefaultBrowser& ChromiumDefaultBrowser() {
  static base::NoDestructor<DefaultBrowserAdapter> default_browser;
  return *default_browser;
}

SelectFilePolicies& ChromiumSelectFilePolicies() {
  static base::NoDestructor<SelectFilePoliciesAdapter> policies;
  return *policies;
}

Favicons& ChromiumFavicons() {
  static base::NoDestructor<FaviconsAdapter> favicons;
  return *favicons;
}

StripDefaults& ChromiumStripDefaults() {
  static base::NoDestructor<StripDefaultsAdapter> strip_defaults;
  return *strip_defaults;
}

}  // namespace cromarc
