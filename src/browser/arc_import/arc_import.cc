// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/arc_import/arc_import.h"

#include <set>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "cromarc/browser/tree/tree.h"

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

void CollectUrls(const bookmarks::BookmarkNode* folder,
                 std::set<std::string>& urls) {
  for (const auto& child : folder->children()) {
    if (child->is_folder()) {
      CollectUrls(child.get(), urls);
    } else {
      urls.insert(UrlKey(child->url()));
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
      const bookmarks::BookmarkNode* item = AddPinned(node, "");
      if (!item) {
        continue;
      }
      if (tree_->SetEssential(Tree::IdOf(item), true)) {
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
  const bookmarks::BookmarkNode* Parent(const std::string& parent) {
    return parent.empty() ? tree_->Root() : tree_->Find(parent);
  }

  size_t End(const std::string& parent) {
    const bookmarks::BookmarkNode* folder = Parent(parent);
    return folder ? folder->children().size() : 0;
  }

  // The folder `title` under `parent`: an existing one of that name, so that
  // importing again fills the same folders, or a new one.
  std::string Folder(const std::string& parent, const std::u16string& title) {
    if (const bookmarks::BookmarkNode* folder = Parent(parent)) {
      for (const auto& child : folder->children()) {
        if (child->is_folder() && child->GetTitle() == title) {
          return Tree::IdOf(child.get());
        }
      }
    }
    const bookmarks::BookmarkNode* created =
        tree_->CreateFolder(parent, End(parent), title);
    if (!created) {
      return parent;
    }
    ++counts_.folders;
    return Tree::IdOf(created);
  }

  const bookmarks::BookmarkNode* AddPinned(const ArcNode& node,
                                           const std::string& parent) {
    if (!pinned_urls_.insert(UrlKey(node.url)).second) {
      ++counts_.skipped;
      return nullptr;
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
