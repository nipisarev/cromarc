// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/arc_import/arc_import.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "cromarc/browser/tree/tree.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cromarc {
namespace {

ArcNode Tab(const std::string& title, const std::string& url) {
  ArcNode node;
  node.title = base::UTF8ToUTF16(title);
  node.url = GURL(url);
  return node;
}

ArcNode Folder(const std::string& title, std::vector<ArcNode> children) {
  ArcNode node;
  node.is_folder = true;
  node.title = base::UTF8ToUTF16(title);
  node.children = std::move(children);
  return node;
}

ArcSpace Space(const std::string& title, std::vector<ArcNode> pinned) {
  ArcSpace space;
  space.title = title;
  space.pinned = std::move(pinned);
  return space;
}

class ArcImportTest : public testing::Test {
 protected:
  ArcImportTest()
      : model_(bookmarks::TestBookmarkClient::CreateModel()),
        tree_(model_.get()) {}

  // "Folder[child, child]" / "title" outline of the tree root.
  std::string Outline(const bookmarks::BookmarkNode* folder) {
    std::string out;
    for (const auto& child : folder->children()) {
      if (!out.empty()) {
        out += ", ";
      }
      out += base::UTF16ToUTF8(child->GetTitle());
      if (child->is_folder()) {
        out += "[" + Outline(child.get()) + "]";
      }
      if (Tree::IsEssential(child.get())) {
        out += "*";
      }
    }
    return out;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  Tree tree_;
};

TEST_F(ArcImportTest, OneArcSpaceGoesToTheRootWithEssentialsFirst) {
  ArcSpace personal = Space("Personal", {
      Folder("Git", {Tab("Tiden APP", "https://github.com/t/app/pulls")}),
      Tab("Mail", "https://mail.example")});
  const ImportCounts counts = ImportIntoTree(
      {&personal}, {Tab("Calendar", "https://calendar.example")}, tree_);
  EXPECT_EQ(Outline(tree_.Root()), "Calendar*, Git[Tiden APP], Mail");
  EXPECT_EQ(counts.folders, 1);
  EXPECT_EQ(counts.pinned, 2);
  EXPECT_EQ(counts.essentials, 1);
}

TEST_F(ArcImportTest, SeveralArcSpacesGetAFolderEach) {
  ArcSpace a = Space("Personal", {Tab("Mail", "https://mail.example")});
  ArcSpace b = Space("Side project", {Tab("Repo", "https://repo.example")});
  ImportIntoTree({&a, &b}, {}, tree_);
  EXPECT_EQ(Outline(tree_.Root()), "Personal[Mail], Side project[Repo]");
}

TEST_F(ArcImportTest, PinnedUrlsAreNotDuplicated) {
  tree_.AddPinned("", 0, u"Mail", GURL("https://mail.example/"));
  ArcSpace personal = Space("Personal", {Tab("Mail", "https://mail.example"),
                                         Tab("New", "https://new.example")});
  const ImportCounts counts = ImportIntoTree({&personal}, {}, tree_);
  EXPECT_EQ(Outline(tree_.Root()), "Mail, New");
  EXPECT_EQ(counts.skipped, 1);
}

TEST_F(ArcImportTest, ImportingAgainMergesIntoTheSameFolders) {
  ArcSpace personal = Space("Personal", {
      Folder("Git", {Tab("Tiden APP", "https://github.com/t/app/pulls"),
                     Folder("Qase", {Tab("Stage", "https://stage.qase.io")})})});
  ImportIntoTree({&personal}, {}, tree_);
  personal.pinned[0].children.push_back(Tab("CLI", "https://cli.example"));
  const ImportCounts counts = ImportIntoTree({&personal}, {}, tree_);
  EXPECT_EQ(Outline(tree_.Root()), "Git[Tiden APP, Qase[Stage], CLI]");
  EXPECT_EQ(counts.folders, 0);
  EXPECT_EQ(counts.pinned, 1);
  EXPECT_EQ(counts.skipped, 2);
}

TEST_F(ArcImportTest, EssentialsStopAtTheLimit) {
  std::vector<ArcNode> essentials;
  for (int i = 0; i < 14; ++i) {
    essentials.push_back(
        Tab("E", "https://e" + std::to_string(i) + ".example"));
  }
  ArcSpace empty = Space("Personal", {});
  const ImportCounts counts = ImportIntoTree({&empty}, essentials, tree_);
  EXPECT_EQ(counts.essentials, 12);
  // The rest come over as pinned items.
  EXPECT_EQ(counts.pinned, 2);
}

}  // namespace
}  // namespace cromarc
