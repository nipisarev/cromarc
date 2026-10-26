// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_DEFAULT_BROWSER_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_DEFAULT_BROWSER_ADAPTER_H_

#include "cromarc/browser/ports/default_browser.h"
#include "cromarc/browser/ports/select_file_policies.h"

namespace cromarc {

// DefaultBrowser on Chrome's shell_integration.
class DefaultBrowserAdapter : public DefaultBrowser {
 public:
  DefaultBrowserAdapter();
  DefaultBrowserAdapter(const DefaultBrowserAdapter&) = delete;
  DefaultBrowserAdapter& operator=(const DefaultBrowserAdapter&) = delete;
  ~DefaultBrowserAdapter() override;

  // DefaultBrowser:
  void CheckIsDefault(base::OnceCallback<void(bool)> done) override;
  void SetAsDefault(base::OnceCallback<void(bool)> done) override;
};

// SelectFilePolicies on ChromeSelectFilePolicy.
class SelectFilePoliciesAdapter : public SelectFilePolicies {
 public:
  SelectFilePoliciesAdapter();
  SelectFilePoliciesAdapter(const SelectFilePoliciesAdapter&) = delete;
  SelectFilePoliciesAdapter& operator=(const SelectFilePoliciesAdapter&) =
      delete;
  ~SelectFilePoliciesAdapter() override;

  // SelectFilePolicies:
  std::unique_ptr<ui::SelectFilePolicy> For(
      content::WebContents* contents) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_DEFAULT_BROWSER_ADAPTER_H_
