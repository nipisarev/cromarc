// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/arc_import/arc_import.h"

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "cromarc/browser/ports/tree.h"

namespace cromarc {

std::string UrlKey(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  std::string spec = url.ReplaceComponents(replacements).spec();
  if (spec.ends_with('/')) {
    spec.pop_back();
  }
  return spec;
}

namespace {

void CollectUrls(const Tree::Node& folder, std::set<std::string>& urls) {
  for (const Tree::Node& child : folder.children) {
    if (child.is_folder) {
      CollectUrls(child, urls);
    } else {
      urls.insert(UrlKey(child.home_url));
    }
  }
}

class Importer {
 public:
  explicit Importer(Tree& tree) : tree_(tree) {
    CollectUrls(tree_->Root(), pinned_urls_);
  }

  void AddEssentials(const std::vector<ArcNode>& essentials) {
    for (const ArcNode& node : essentials) {
      const std::optional<std::string> item = AddPinned(node, "");
      if (!item) {
        continue;
      }
      if (tree_->SetEssential(*item, true)) {
        ++counts_.essentials;
      } else {
        ++counts_.pinned;
      }
    }
  }

  void AddNodes(const std::vector<ArcNode>& nodes, const std::string& parent) {
    for (const ArcNode& node : nodes) {
      if (!node.is_folder) {
        if (AddPinned(node, parent)) {
          ++counts_.pinned;
        }
        continue;
      }
      AddNodes(node.children, Folder(parent, node.title));
    }
  }

  std::string AddSpaceFolder(const ArcSpace& space) {
    return Folder("", base::UTF8ToUTF16(space.title));
  }

  ImportCounts counts() const { return counts_; }

 private:
  std::optional<Tree::Node> Parent(const std::string& parent) {
    return parent.empty() ? tree_->Root() : tree_->Find(parent);
  }

  size_t End(const std::string& parent) {
    const std::optional<Tree::Node> folder = Parent(parent);
    return folder ? folder->children.size() : 0;
  }

  // The folder `title` under `parent`: an existing one of that name, so that
  // importing again fills the same folders, or a new one.
  std::string Folder(const std::string& parent, const std::u16string& title) {
    if (const std::optional<Tree::Node> folder = Parent(parent)) {
      for (const Tree::Node& child : folder->children) {
        if (child.is_folder && child.title == title) {
          return child.id;
        }
      }
    }
    const std::optional<std::string> created =
        tree_->CreateFolder(parent, End(parent), title);
    if (!created) {
      return parent;
    }
    ++counts_.folders;
    return *created;
  }

  std::optional<std::string> AddPinned(const ArcNode& node,
                                       const std::string& parent) {
    if (!pinned_urls_.insert(UrlKey(node.url)).second) {
      ++counts_.skipped;
      return std::nullopt;
    }
    return tree_->AddPinned(parent, End(parent), node.title, node.url);
  }

  const raw_ref<Tree> tree_;
  std::set<std::string> pinned_urls_;
  ImportCounts counts_;
};

}  // namespace

ImportCounts ImportIntoTree(const std::vector<const ArcSpace*>& spaces,
                            const std::vector<ArcNode>& essentials,
                            Tree& tree) {
  Importer importer(tree);
  importer.AddEssentials(essentials);
  for (const ArcSpace* space : spaces) {
    const std::string parent =
        spaces.size() > 1 ? importer.AddSpaceFolder(*space) : std::string();
    importer.AddNodes(space->pinned, parent);
  }
  return importer.counts();
}

}  // namespace cromarc
