// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAVICONS_H_
#define CROMARC_BROWSER_PORTS_FAVICONS_H_

#include <string>

#include "base/functional/callback_forward.h"

class GURL;
class Profile;

namespace base {
class CancelableTaskTracker;
}  // namespace base

namespace cromarc {

// The favicons Chrome keeps for the pages a profile visited.
class Favicons {
 public:
  virtual ~Favicons() = default;

  // `page`'s favicon (its site's, without one of its own) as a data: URL of a
  // PNG about `size_px` wide; "" when there is none. Cancelled with
  // `tracker`.
  virtual void GetForPage(Profile* profile,
                          const GURL& page,
                          int size_px,
                          base::OnceCallback<void(std::string)> done,
                          base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAVICONS_H_
