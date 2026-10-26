// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SHARED_SIDEBAR_STATE_H_
#define CROMARC_BROWSER_SPACES_SHARED_SIDEBAR_STATE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"

namespace cromarc {

class VerticalStrip;

// Zen keeps one sidebar in its window whichever space it shows: compact or
// not, and as wide. The space windows are that one window here, so a change
// in one goes to the others and to the windows the spaces open later.
class SharedSidebarState {
 public:
  struct State {
    bool collapsed = false;
    int width = 0;

    bool operator==(const State&) const = default;
  };

  // `on_changed` runs once a change has gone to every strip.
  explicit SharedSidebarState(base::RepeatingClosure on_changed);
  SharedSidebarState(const SharedSidebarState&) = delete;
  SharedSidebarState& operator=(const SharedSidebarState&) = delete;
  ~SharedSidebarState();

  // `strip` takes the shared state, or gives it when there is none yet, and
  // shares its changes while the result lives.
  [[nodiscard]] base::ScopedClosureRunner Add(VerticalStrip& strip);

  const std::optional<State>& state() const { return state_; }

 private:
  void Remove(VerticalStrip* strip);
  void Share(VerticalStrip* from);
  // `strip` as the shared state has it.
  void Apply(VerticalStrip& strip);

  const base::RepeatingClosure on_changed_;
  std::map<VerticalStrip*, std::vector<base::CallbackListSubscription>>
      strips_;
  std::optional<State> state_;
  // While the shared state goes to the strips, their changes are its own.
  bool sharing_ = false;
  base::WeakPtrFactory<SharedSidebarState> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SHARED_SIDEBAR_STATE_H_
