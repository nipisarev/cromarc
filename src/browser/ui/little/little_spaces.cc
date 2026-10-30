// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_spaces.h"

#include <utility>

#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/ui/little/little_bar.h"

namespace cromarc {

LittleSpaces::LittleSpaces() = default;
LittleSpaces::~LittleSpaces() = default;

std::unique_ptr<views::View> LittleSpaces::MakeBar(LittleWindow& window) {
  return std::make_unique<LittleBar>(&window);
}

void LittleSpaces::ShowSpace(
    const std::string& space_id,
    base::OnceCallback<void(BrowserWindowInterface*)> then) {
  SpaceController::Get()->ShowSpace(space_id, std::move(then));
}

void LittleSpaces::LoadSpace(const std::string& space_id,
                             base::OnceCallback<void(Profile*)> then) {
  SpaceController::Get()->LoadSpace(space_id, std::move(then));
}

void LittleSpaces::Archive(Profile* profile,
                           ArchiveEntry entry,
                           bool session_ending) {
  ArchiveService* archive = ArchiveService::Get(profile);
  if (!archive) {
    return;
  }
  if (session_ending) {
    archive->AddAtSessionEnd(entry);
  } else {
    archive->Add(std::move(entry));
  }
}

}  // namespace cromarc
