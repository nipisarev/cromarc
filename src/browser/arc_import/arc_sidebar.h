// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_ARC_IMPORT_ARC_SIDEBAR_H_
#define CROMARC_BROWSER_ARC_IMPORT_ARC_SIDEBAR_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace cromarc {

// A folder or a pinned tab of Arc's sidebar.
struct ArcNode {
  ArcNode();
  ArcNode(const ArcNode&);
  ArcNode(ArcNode&&);
  ArcNode& operator=(const ArcNode&);
  ArcNode& operator=(ArcNode&&);
  ~ArcNode();

  bool is_folder = false;
  std::u16string title;
  GURL url;
  std::vector<ArcNode> children;
};

struct ArcSpace {
  ArcSpace();
  ArcSpace(const ArcSpace&);
  ArcSpace(ArcSpace&&);
  ArcSpace& operator=(const ArcSpace&);
  ArcSpace& operator=(ArcSpace&&);
  ~ArcSpace();

  std::string arc_id;
  std::string title;
  // Emoji; empty for Arc's built-in icons, which have no emoji.
  std::string icon;
  // The mid tone of the space's theme.
  std::optional<SkColor> color;
  // The Arc profile directory the space is bound to; "" for Arc's default.
  std::string arc_profile;
  // Folders and pinned tabs; Arc's Today tabs are not imported.
  std::vector<ArcNode> pinned;
};

struct ArcSidebar {
  ArcSidebar();
  ArcSidebar(const ArcSidebar&);
  ArcSidebar(ArcSidebar&&);
  ArcSidebar& operator=(const ArcSidebar&);
  ArcSidebar& operator=(ArcSidebar&&);
  ~ArcSidebar();

  std::vector<ArcSpace> spaces;
  // Essentials ("top apps") per Arc profile directory, "" for the default.
  std::map<std::string, std::vector<ArcNode>> essentials;
};

// Reads Arc's ~/Library/Application Support/Arc/StorableSidebar.json (design,
// component 8; the format as Stedding's importer, patches 0026 and 0080,
// read it). Null for anything else.
std::optional<ArcSidebar> ParseArcSidebar(std::string_view json);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_ARC_IMPORT_ARC_SIDEBAR_H_
