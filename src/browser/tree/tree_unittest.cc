// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class TreeTest : public testing::Test {
 protected:
  TreeTest()
      : model_(bookmarks::TestBookmarkClient::CreateModel()),
        tree_(model_.get()) {}

  std::vector<std::string> Titles(const BookmarkNode* folder) {
    std::vector<std::string> titles;
    for (const auto& child : folder->children()) {
      titles.push_back(base::UTF16ToUTF8(child->GetTitle()));
    }
    return titles;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BookmarkModel> model_;
  Tree tree_;
};

TEST_F(TreeTest, RootIsAFolderInOtherBookmarksCreatedOnce) {
  const BookmarkNode* root = tree_.Root();
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_folder());
  EXPECT_EQ(root->parent(), model_->other_node());
  EXPECT_EQ(tree_.Root(), root);
  EXPECT_EQ(model_->other_node()->children().size(), 1u);
}

TEST_F(TreeTest, RootIsFoundAgainAfterARenameAndByANewTree) {
  const BookmarkNode* root = tree_.Root();
  model_->SetTitle(root, u"Renamed", bookmarks::metrics::BookmarkEditSource::kUser);
  Tree other(model_.get());
  EXPECT_EQ(other.Root(), root);
}

TEST_F(TreeTest, FoldersAndPinnedItemsNestAtTheirPositions) {
  const BookmarkNode* git = tree_.CreateFolder("", 0, u"Git");
  ASSERT_TRUE(git);
  const std::string git_id = Tree::IdOf(git);
  ASSERT_TRUE(tree_.AddPinned(git_id, 0, u"Acme App", GURL("https://github.com/a/pulls")));
  ASSERT_TRUE(tree_.AddPinned(git_id, 0, u"Acme CLI", GURL("https://github.com/b/pulls")));
  const BookmarkNode* acme = tree_.CreateFolder(git_id, 2, u"Acme");
  ASSERT_TRUE(acme);
  EXPECT_EQ(Titles(git), (std::vector<std::string>{"Acme CLI", "Acme App", "Acme"}));
  EXPECT_EQ(tree_.Find(Tree::IdOf(acme)), acme);
}

TEST_F(TreeTest, UnknownParentIsRefused) {
  EXPECT_FALSE(tree_.CreateFolder("no-such-id", 0, u"X"));
  EXPECT_FALSE(tree_.AddPinned("no-such-id", 0, u"X", GURL("https://x")));
}

TEST_F(TreeTest, NodesOutsideTheTreeAreNotFound) {
  const BookmarkNode* outside = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"Elsewhere", GURL("https://e"));
  EXPECT_FALSE(tree_.Find(Tree::IdOf(outside)));
  EXPECT_FALSE(tree_.Remove(Tree::IdOf(outside)));
  EXPECT_EQ(model_->bookmark_bar_node()->children().size(), 1u);
}

TEST_F(TreeTest, MoveReordersAndReparents) {
  const BookmarkNode* a = tree_.CreateFolder("", 0, u"A");
  const BookmarkNode* b = tree_.CreateFolder("", 1, u"B");
  const BookmarkNode* item = tree_.AddPinned("", 2, u"Item", GURL("https://i"));
  ASSERT_TRUE(tree_.Move(Tree::IdOf(item), "", 0));
  EXPECT_EQ(Titles(tree_.Root()), (std::vector<std::string>{"Item", "A", "B"}));
  ASSERT_TRUE(tree_.Move(Tree::IdOf(b), Tree::IdOf(a), 0));
  EXPECT_EQ(b->parent(), a);
}

TEST_F(TreeTest, AFolderCannotMoveIntoItself) {
  const BookmarkNode* a = tree_.CreateFolder("", 0, u"A");
  const BookmarkNode* inner = tree_.CreateFolder(Tree::IdOf(a), 0, u"Inner");
  EXPECT_FALSE(tree_.Move(Tree::IdOf(a), Tree::IdOf(inner), 0));
  EXPECT_FALSE(tree_.Move(Tree::IdOf(a), Tree::IdOf(a), 0));
  EXPECT_EQ(a->parent(), tree_.Root());
}

TEST_F(TreeTest, FlagsAndHomeUrlAreStored) {
  const BookmarkNode* folder = tree_.CreateFolder("", 0, u"F");
  const BookmarkNode* item = tree_.AddPinned("", 1, u"I", GURL("https://home"));
  EXPECT_FALSE(Tree::IsCollapsed(folder));
  ASSERT_TRUE(tree_.SetCollapsed(Tree::IdOf(folder), true));
  EXPECT_TRUE(Tree::IsCollapsed(folder));
  ASSERT_TRUE(tree_.SetEssential(Tree::IdOf(item), true));
  EXPECT_TRUE(Tree::IsEssential(item));
  ASSERT_TRUE(tree_.SetHomeUrl(Tree::IdOf(item), GURL("https://new-home")));
  EXPECT_EQ(item->url(), GURL("https://new-home"));
  ASSERT_TRUE(tree_.Rename(Tree::IdOf(item), u"Renamed"));
  EXPECT_EQ(item->GetTitle(), u"Renamed");
  EXPECT_FALSE(tree_.SetEssential(Tree::IdOf(folder), true));
  EXPECT_FALSE(tree_.SetHomeUrl(Tree::IdOf(folder), GURL("https://x")));
}

TEST_F(TreeTest, EssentialsAreCappedAtTwelve) {
  for (int i = 0; i < 12; ++i) {
    const BookmarkNode* item = tree_.AddPinned(
        "", i, u"E", GURL("https://e" + base::NumberToString(i) + ".com"));
    ASSERT_TRUE(tree_.SetEssential(Tree::IdOf(item), true));
  }
  const BookmarkNode* extra = tree_.AddPinned("", 12, u"E", GURL("https://x"));
  EXPECT_FALSE(tree_.SetEssential(Tree::IdOf(extra), true));
  EXPECT_EQ(tree_.EssentialCount(), 12u);
}

TEST_F(TreeTest, RemoveDeletesTheNodeAndItsChildren) {
  const BookmarkNode* folder = tree_.CreateFolder("", 0, u"F");
  tree_.AddPinned(Tree::IdOf(folder), 0, u"I", GURL("https://i"));
  ASSERT_TRUE(tree_.Remove(Tree::IdOf(folder)));
  EXPECT_TRUE(tree_.Root()->children().empty());
  EXPECT_FALSE(tree_.Remove(Tree::IdOf(tree_.Root())));
}

TEST_F(TreeTest, ChangesUnderTheRootAreReported) {
  int changes = 0;
  auto subscription = tree_.RegisterOnChanged(
      base::BindRepeating([](int* count) { ++*count; }, &changes));
  tree_.CreateFolder("", 0, u"F");
  EXPECT_GT(changes, 0);
  const int before = changes;
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Elsewhere", GURL("https://e"));
  EXPECT_EQ(changes, before);
}

}  // namespace
}  // namespace cromarc
