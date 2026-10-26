// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_SELECT_FILE_POLICIES_H_
#define CROMARC_BROWSER_PORTS_SELECT_FILE_POLICIES_H_

#include <memory>

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class SelectFilePolicy;
}  // namespace ui

namespace cromarc {

// What may open a file dialog, as Chrome's policies say.
class SelectFilePolicies {
 public:
  virtual ~SelectFilePolicies() = default;

  // The policy for a file dialog `contents` opens.
  virtual std::unique_ptr<ui::SelectFilePolicy> For(
      content::WebContents* contents) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_SELECT_FILE_POLICIES_H_
