// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_SPACES_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_SPACES_H_

#include "base/memory/raw_ref.h"
#include "cromarc/browser/ui/glance/glance_registry.h"

class Profile;

namespace cromarc {

class Tabs;
class Windows;

// What a glance needs of the space its tab is in: the tab's place in the
// space's tree, and the space's archive.
class GlanceSpaces : public GlanceRegistry::Delegate {
 public:
  GlanceSpaces(Tabs& tabs, Windows& windows);
  GlanceSpaces(const GlanceSpaces&) = delete;
  GlanceSpaces& operator=(const GlanceSpaces&) = delete;
  ~GlanceSpaces() override;

  // GlanceRegistry::Delegate:
  TabKind KindOf(TabId tab) override;
  void Archive(TabId glance, bool session_ending) override;

 private:
  Profile* ProfileOf(TabId tab);

  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_SPACES_H_
