// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_LITTLE_LITTLE_SPACES_H_
#define CROMARC_BROWSER_UI_LITTLE_LITTLE_SPACES_H_

#include "cromarc/browser/ui/little/little_window.h"

namespace cromarc {

// Little windows' bar, spaces and archive: LittleBar, SpaceController and
// each profile's ArchiveService.
class LittleSpaces : public LittleWindow::Delegate {
 public:
  LittleSpaces();
  ~LittleSpaces() override;

  // LittleWindow::Delegate:
  std::unique_ptr<views::View> MakeBar(LittleWindow& window) override;
  void ShowSpace(
      const std::string& space_id,
      base::OnceCallback<void(BrowserWindowInterface*)> then) override;
  void LoadSpace(const std::string& space_id,
                 base::OnceCallback<void(Profile*)> then) override;
  void Archive(Profile* profile,
               ArchiveEntry entry,
               bool session_ending) override;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_LITTLE_LITTLE_SPACES_H_
