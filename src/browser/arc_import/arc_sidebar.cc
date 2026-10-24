// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/arc_import/arc_sidebar.h"

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace cromarc {

ArcNode::ArcNode() = default;
ArcNode::ArcNode(const ArcNode&) = default;
ArcNode::ArcNode(ArcNode&&) = default;
ArcNode& ArcNode::operator=(const ArcNode&) = default;
ArcNode& ArcNode::operator=(ArcNode&&) = default;
ArcNode::~ArcNode() = default;

ArcSpace::ArcSpace() = default;
ArcSpace::ArcSpace(const ArcSpace&) = default;
ArcSpace::ArcSpace(ArcSpace&&) = default;
ArcSpace& ArcSpace::operator=(const ArcSpace&) = default;
ArcSpace& ArcSpace::operator=(ArcSpace&&) = default;
ArcSpace::~ArcSpace() = default;

ArcSidebar::ArcSidebar() = default;
ArcSidebar::ArcSidebar(const ArcSidebar&) = default;
ArcSidebar::ArcSidebar(ArcSidebar&&) = default;
ArcSidebar& ArcSidebar::operator=(const ArcSidebar&) = default;
ArcSidebar& ArcSidebar::operator=(ArcSidebar&&) = default;
ArcSidebar::~ArcSidebar() = default;

namespace {

// Folders nest; Arc caps nothing, so guard against cycles.
constexpr int kMaxDepth = 32;

std::string StringAt(const base::DictValue& dict, std::string_view key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

// Arc's lists interleave an id string with the dict it names.
std::vector<const base::DictValue*> DictsOf(const base::ListValue* list) {
  std::vector<const base::DictValue*> dicts;
  if (list) {
    for (const base::Value& value : *list) {
      if (const base::DictValue* dict = value.GetIfDict()) {
        dicts.push_back(dict);
      }
    }
  }
  return dicts;
}

// The profile directory in Arc's binding {"default": true} or
// {"custom": {"_0": {"machineID": ..., "directoryBasename": ...}}}.
std::string ArcProfileOf(const base::Value* binding) {
  const base::DictValue* dict = binding ? binding->GetIfDict() : nullptr;
  if (!dict || dict->contains("default")) {
    return std::string();
  }
  const std::string* basename =
      dict->FindStringByDottedPath("custom._0.directoryBasename");
  return basename ? *basename : std::string();
}

std::optional<SkColor> ThemeColor(const base::DictValue& space) {
  const base::DictValue* tone = space.FindDictByDottedPath(
      "customInfo.windowTheme.primaryColorPalette.midTone");
  if (!tone) {
    return std::nullopt;
  }
  const auto channel = [tone](std::string_view key) {
    const double value = tone->FindDouble(key).value_or(0.0);
    return static_cast<U8CPU>(std::clamp(value, 0.0, 1.0) * 255.0 + 0.5);
  };
  return SkColorSetRGB(channel("red"), channel("green"), channel("blue"));
}

class Builder {
 public:
  explicit Builder(const base::DictValue& sidebar) {
    for (const base::DictValue* item : DictsOf(sidebar.FindList("items"))) {
      const std::string id = StringAt(*item, "id");
      if (!id.empty()) {
        items_.emplace(id, item);
      }
    }
  }

  // The nodes under the item `id`, in Arc's order.
  std::vector<ArcNode> ChildrenOf(const std::string& id, int depth = 0) {
    std::vector<ArcNode> nodes;
    auto it = items_.find(id);
    if (it == items_.end() || depth > kMaxDepth) {
      return nodes;
    }
    const base::ListValue* children = it->second->FindList("childrenIds");
    if (!children) {
      return nodes;
    }
    for (const base::Value& child_id : *children) {
      if (!child_id.is_string()) {
        continue;
      }
      auto child = items_.find(child_id.GetString());
      if (child == items_.end()) {
        continue;
      }
      AddNode(*child->second, child_id.GetString(), depth, nodes);
    }
    return nodes;
  }

 private:
  void AddNode(const base::DictValue& item,
               const std::string& id,
               int depth,
               std::vector<ArcNode>& nodes) {
    const base::DictValue* data = item.FindDict("data");
    if (!data) {
      return;
    }
    const std::string user_title = StringAt(item, "title");
    if (const base::DictValue* tab = data->FindDict("tab")) {
      ArcNode node;
      node.url = GURL(StringAt(*tab, "savedURL"));
      if (!node.url.is_valid()) {
        return;
      }
      // A renamed tab keeps the user's name.
      node.title = base::UTF8ToUTF16(
          user_title.empty() ? StringAt(*tab, "savedTitle") : user_title);
      nodes.push_back(std::move(node));
      return;
    }
    if (data->contains("list")) {
      ArcNode node;
      node.is_folder = true;
      node.title = base::UTF8ToUTF16(user_title);
      node.children = ChildrenOf(id, depth + 1);
      nodes.push_back(std::move(node));
      return;
    }
    // A split view comes across as its tabs.
    if (data->contains("splitView")) {
      for (ArcNode& child : ChildrenOf(id, depth + 1)) {
        nodes.push_back(std::move(child));
      }
    }
  }

  std::map<std::string, raw_ptr<const base::DictValue>> items_;
};

}  // namespace

std::optional<ArcSidebar> ParseArcSidebar(std::string_view json) {
  const std::optional<base::Value> root =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!root || !root->is_dict()) {
    return std::nullopt;
  }
  const base::ListValue* containers =
      root->GetDict().FindListByDottedPath("sidebar.containers");
  if (!containers) {
    return std::nullopt;
  }
  const base::DictValue* sidebar = nullptr;
  for (const base::Value& container : *containers) {
    const base::DictValue* dict = container.GetIfDict();
    if (dict && dict->FindList("spaces") && dict->FindList("items")) {
      sidebar = dict;
      break;
    }
  }
  if (!sidebar) {
    return std::nullopt;
  }

  Builder builder(*sidebar);
  ArcSidebar result;
  for (const base::DictValue* dict : DictsOf(sidebar->FindList("spaces"))) {
    ArcSpace space;
    space.arc_id = StringAt(*dict, "id");
    if (space.arc_id.empty()) {
      continue;
    }
    space.title = StringAt(*dict, "title");
    if (const base::DictValue* icon =
            dict->FindDictByDottedPath("customInfo.iconType")) {
      for (const char* key : {"emoji_v2", "emoji"}) {
        space.icon = StringAt(*icon, key);
        if (!space.icon.empty()) {
          break;
        }
      }
    }
    space.color = ThemeColor(*dict);
    space.arc_profile = ArcProfileOf(dict->Find("profile"));
    // "pinned" or "unpinned", then the id of that container.
    if (const base::ListValue* ids = dict->FindList("containerIDs")) {
      bool next_is_pinned = false;
      for (const base::Value& entry : *ids) {
        if (!entry.is_string()) {
          continue;
        }
        const std::string& text = entry.GetString();
        if (text == "pinned" || text == "unpinned") {
          next_is_pinned = text == "pinned";
          continue;
        }
        if (next_is_pinned) {
          space.pinned = builder.ChildrenOf(text);
        }
        next_is_pinned = false;
      }
    }
    result.spaces.push_back(std::move(space));
  }

  // A profile binding, then the id of that profile's top-apps container.
  if (const base::ListValue* top = sidebar->FindList("topAppsContainerIDs")) {
    std::optional<std::string> profile;
    for (const base::Value& entry : *top) {
      if (entry.is_dict()) {
        profile = ArcProfileOf(&entry);
        continue;
      }
      if (entry.is_string() && profile) {
        std::vector<ArcNode>& essentials = result.essentials[*profile];
        for (ArcNode& node : builder.ChildrenOf(entry.GetString())) {
          if (!node.is_folder) {
            essentials.push_back(std::move(node));
          }
        }
      }
      profile.reset();
    }
  }
  return result;
}

}  // namespace cromarc
