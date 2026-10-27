// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/webui_strings.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "content/public/browser/web_ui_data_source.h"
#include "cromarc/strings/grit/cromarc_strings_map.h"

namespace cromarc {

std::string WebUIStringKey(std::string_view message_name) {
  constexpr std::string_view kPrefix = "IDS_CROMARC_";
  CHECK(base::StartsWith(message_name, kPrefix)) << message_name;
  std::string key;
  bool word_start = false;
  for (char c : message_name.substr(kPrefix.size())) {
    if (c == '_') {
      word_start = true;
      continue;
    }
    key += word_start ? c : base::ToLowerASCII(c);
    word_start = false;
  }
  return key;
}

std::vector<std::pair<std::string, int>> CromarcWebUIStrings() {
  std::vector<std::pair<std::string, int>> strings;
  for (const webui::ResourcePath& message : kCromarcStrings) {
    strings.emplace_back(WebUIStringKey(message.path), message.id);
  }
  return strings;
}

void AddCromarcStrings(content::WebUIDataSource* source,
                       std::string_view app_locale) {
  for (const auto& [key, id] : CromarcWebUIStrings()) {
    source->AddLocalizedString(key, id);
  }
  source->AddString("locale", app_locale);
}

}  // namespace cromarc
