// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_ADAPTERS_H_
#define CROMARC_BROWSER_CHROMIUM_ADAPTERS_H_

namespace cromarc {

class AppLifecycle;
class Profiles;
class StripDefaults;
class TabStripApi;
class Tabs;
class Theme;
class Windows;

// The process-wide ports on Chromium, for the composition roots: keyed
// service factories and InstallSidebar().
Tabs& ChromiumTabs();
Windows& ChromiumWindows();
AppLifecycle& ChromiumAppLifecycle();
TabStripApi& ChromiumTabStripApi();
Profiles& ChromiumProfiles();
Theme& ChromiumTheme();
StripDefaults& ChromiumStripDefaults();

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_ADAPTERS_H_
