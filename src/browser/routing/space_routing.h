// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ROUTING_SPACE_ROUTING_H_
#define CROMARC_BROWSER_ROUTING_SPACE_ROUTING_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "cromarc/browser/routing/routes.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cromarc {

// Which space links from other apps open in (design, component 5; Zen's
// space routing): rules shared by all spaces, so they live beside the
// profiles in "Cromarc Routes.json", in Zen's format.
class SpaceRouting {
 public:
  // The browser-process singleton (chromium/space_routing_get.cc).
  static SpaceRouting* Get();

  // The rules live in the file at `path`.
  explicit SpaceRouting(const base::FilePath& path);
  ~SpaceRouting();

  SpaceRouting(const SpaceRouting&) = delete;
  SpaceRouting& operator=(const SpaceRouting&) = delete;

  // Runs `callback` once the file has been read (at once after that).
  void WhenLoaded(base::OnceClosure callback);
  const Routes& routes() const { return routes_; }

  void AddRoute(const std::string& reference,
                Route::Match match,
                const std::string& space);
  void UpdateRoute(const std::string& id,
                   const std::string& reference,
                   Route::Match match,
                   const std::string& space);
  void RemoveRoute(const std::string& id);
  void SetDefaultExternal(const std::string& space);

  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback);

 private:
  void OnRead(std::optional<Routes> routes);
  void Save();

  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::FilePath path_;
  bool loaded_ = false;
  Routes routes_;
  std::vector<base::OnceClosure> pending_;
  base::RepeatingClosureList on_changed_;
  base::WeakPtrFactory<SpaceRouting> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ROUTING_SPACE_ROUTING_H_
