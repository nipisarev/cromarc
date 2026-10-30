// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance_spaces.h"

#include <optional>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/tree.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/tree/tree_service.h"

namespace cromarc {

GlanceSpaces::GlanceSpaces(Tabs& tabs, Windows& windows)
    : tabs_(tabs), windows_(windows) {}

GlanceSpaces::~GlanceSpaces() = default;

Profile* GlanceSpaces::ProfileOf(TabId tab) {
  BrowserWindowInterface* window = tabs_->GetWindow(tab);
  return window ? windows_->ProfileOf(window) : nullptr;
}

TabKind GlanceSpaces::KindOf(TabId tab) {
  Profile* profile = ProfileOf(tab);
  TreeService* tree = profile ? TreeService::Get(profile) : nullptr;
  const std::optional<std::string> item =
      tree ? tree->GetItemForTab(tab) : std::nullopt;
  if (!item) {
    return TabKind::kToday;
  }
  const std::optional<Tree::Node> node = tree->tree().Find(*item);
  return node && node->essential ? TabKind::kEssential : TabKind::kPinned;
}

void GlanceSpaces::Archive(TabId glance, bool session_ending) {
  Profile* profile = ProfileOf(glance);
  ArchiveService* archive = profile ? ArchiveService::Get(profile) : nullptr;
  if (!archive) {
    return;
  }
  ArchiveEntry entry;
  // As a tab that goes to the archive: a page still loading goes under the
  // address it is loading.
  entry.url = tabs_->GetLastCommittedUrl(glance);
  if (entry.url.is_empty()) {
    entry.url = tabs_->GetUrl(glance);
  }
  entry.title = tabs_->GetTitle(glance);
  entry.closed_at = base::Time::Now();
  entry.reason = "glance";
  if (!entry.url.is_valid()) {
    return;
  }
  if (session_ending) {
    archive->AddAtSessionEnd(entry);
  } else {
    archive->Add(std::move(entry));
  }
}

}  // namespace cromarc
