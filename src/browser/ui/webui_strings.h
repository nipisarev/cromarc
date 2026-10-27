// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_WEBUI_STRINGS_H_
#define CROMARC_BROWSER_UI_WEBUI_STRINGS_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace content {
class WebUIDataSource;
}

namespace cromarc {

// The loadTimeData key of a message: IDS_CROMARC_NEW_TAB is newTab.
std::string WebUIStringKey(std::string_view message_name);

// Every message of //cromarc/strings with its key.
std::vector<std::pair<std::string, int>> CromarcWebUIStrings();

// Gives a page every Cromarc string, in the locale Chromium loaded, and that
// locale as "locale" (loadTimeData's own "language" has no region, so en-GB
// dates would read as American).
void AddCromarcStrings(content::WebUIDataSource* source,
                       std::string_view app_locale);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_WEBUI_STRINGS_H_
