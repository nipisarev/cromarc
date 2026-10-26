// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/default_browser_adapter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"

namespace cromarc {

namespace {

base::OnceCallback<void(shell_integration::DefaultWebClientState)> IsDefault(
    base::OnceCallback<void(bool)> done) {
  return base::BindOnce(
      [](base::OnceCallback<void(bool)> done,
         shell_integration::DefaultWebClientState state) {
        std::move(done).Run(state == shell_integration::IS_DEFAULT);
      },
      std::move(done));
}

}  // namespace

DefaultBrowserAdapter::DefaultBrowserAdapter() = default;
DefaultBrowserAdapter::~DefaultBrowserAdapter() = default;

void DefaultBrowserAdapter::CheckIsDefault(
    base::OnceCallback<void(bool)> done) {
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartCheckIsDefault(IsDefault(std::move(done)));
}

void DefaultBrowserAdapter::SetAsDefault(base::OnceCallback<void(bool)> done) {
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartSetAsDefault(IsDefault(std::move(done)));
}

SelectFilePoliciesAdapter::SelectFilePoliciesAdapter() = default;
SelectFilePoliciesAdapter::~SelectFilePoliciesAdapter() = default;

std::unique_ptr<ui::SelectFilePolicy> SelectFilePoliciesAdapter::For(
    content::WebContents* contents) {
  return std::make_unique<ChromeSelectFilePolicy>(contents);
}

}  // namespace cromarc
