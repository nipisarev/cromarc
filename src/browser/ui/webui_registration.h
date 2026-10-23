// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_WEBUI_REGISTRATION_H_
#define CROMARC_BROWSER_UI_WEBUI_REGISTRATION_H_

namespace content {
class WebUIBrowserInterfaceBrokerRegistry;
class WebUIConfigMap;
}  // namespace content

namespace cromarc {

// Hook H4, called from RegisterChromeWebUIConfigs().
void RegisterWebUIConfigs(content::WebUIConfigMap& map);

// Hook H4, called from
// PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop().
void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_WEBUI_REGISTRATION_H_
