// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ARC_IMPORT_ARC_IMPORT_H_
#define CROMARC_BROWSER_ARC_IMPORT_ARC_IMPORT_H_

#include <string>
#include <vector>

#include "cromarc/browser/arc_import/arc_sidebar.h"

namespace cromarc {

class Tree;

struct ImportCounts {
  int folders = 0;
  int pinned = 0;
  int essentials = 0;
  // Today tabs opened.
  int today = 0;
  // Already pinned in the tree or open (same URL), left out.
  int skipped = 0;
};

// URLs compare without the fragment and a trailing slash, as the sidebar does.
std::string UrlKey(const GURL& url);

// Brings Arc spaces chosen for one Cromarc space into its tree: essentials
// first (up to the limit, the rest as pinned items), then each Arc space's
// folders and pinned tabs, at the root for one Arc space or in a folder named
// after it for several. A URL already pinned anywhere in the tree is skipped
// and folders of the same name under the same parent are reused, so
// importing again adds only what is new.
ImportCounts ImportIntoTree(const std::vector<const ArcSpace*>& spaces,
                            const std::vector<ArcNode>& essentials,
                            Tree& tree);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ARC_IMPORT_ARC_IMPORT_H_
