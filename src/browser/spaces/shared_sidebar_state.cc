// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/shared_sidebar_state.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "cromarc/browser/ports/vertical_strip.h"

namespace cromarc {

SharedSidebarState::SharedSidebarState(base::RepeatingClosure on_changed)
    : on_changed_(std::move(on_changed)) {}

SharedSidebarState::~SharedSidebarState() = default;

base::ScopedClosureRunner SharedSidebarState::Add(VerticalStrip& strip) {
  DCHECK(!strips_.contains(&strip));
  std::vector<base::CallbackListSubscription>& subscriptions = strips_[&strip];
  subscriptions.push_back(strip.OnCollapseChanged(
      base::BindRepeating(&SharedSidebarState::Share, base::Unretained(this),
                          base::Unretained(&strip))));
  // A width the user drags is shared once the drag ends.
  subscriptions.push_back(strip.OnResizeEnded(
      base::BindRepeating(&SharedSidebarState::Share, base::Unretained(this),
                          base::Unretained(&strip))));
  if (state_) {
    base::AutoReset<bool> sharing(&sharing_, true);
    Apply(strip);
  } else {
    Share(&strip);
  }
  return base::ScopedClosureRunner(
      base::BindOnce(&SharedSidebarState::Remove, weak_factory_.GetWeakPtr(),
                     base::Unretained(&strip)));
}

void SharedSidebarState::Remove(VerticalStrip* strip) {
  strips_.erase(strip);
}

void SharedSidebarState::Share(VerticalStrip* from) {
  if (sharing_ || from->IsResizing()) {
    return;
  }
  // Collapsing counts as collapsed: that is where it is going.
  const State state{.collapsed = from->IsCollapsedOrCollapsing(),
                    .width = from->ExpandedWidth()};
  if (state_ == state) {
    return;
  }
  state_ = state;
  base::AutoReset<bool> sharing(&sharing_, true);
  for (const auto& [strip, subscriptions] : strips_) {
    if (strip != from) {
      Apply(*strip);
    }
  }
  on_changed_.Run();
}

void SharedSidebarState::Apply(VerticalStrip& strip) {
  if (strip.ExpandedWidth() != state_->width) {
    strip.SetExpandedWidth(state_->width);
  }
  if (strip.IsCollapsedOrCollapsing() != state_->collapsed) {
    strip.RequestCollapse(state_->collapsed);
  }
}

}  // namespace cromarc
