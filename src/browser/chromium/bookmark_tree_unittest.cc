// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/bookmark_tree.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "cromarc/browser/ports/fake_tree.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

std::vector<std::string> Titles(const Tree::Node& folder) {
  std::vector<std::string> titles;
  for (const Tree::Node& child : folder.children) {
    titles.push_back(base::UTF16ToUTF8(child.title));
  }
  return titles;
}

// Tree's contract, which every implementation keeps: the bookmarks one and
// the fake that other tests use.
struct BookmarkTreeMaker {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  BookmarkTree tree{model.get()};
};

struct FakeTreeMaker {
  FakeTree tree;
};

template <typename Maker>
class TreeContractTest : public testing::Test {
 protected:
  Tree& tree() { return maker_.tree; }

  base::test::TaskEnvironment task_environment_;
  Maker maker_;
};

using Makers = testing::Types<BookmarkTreeMaker, FakeTreeMaker>;
TYPED_TEST_SUITE(TreeContractTest, Makers);

TYPED_TEST(TreeContractTest, FoldersAndPinnedItemsNestAtTheirPositions) {
  Tree& tree = this->tree();
  const std::optional<std::string> git = tree.CreateFolder("", 0, u"Git");
  ASSERT_TRUE(git);
  ASSERT_TRUE(tree.AddPinned(*git, 0, u"Acme App",
                             GURL("https://github.com/a/pulls")));
  ASSERT_TRUE(tree.AddPinned(*git, 0, u"Acme CLI",
                             GURL("https://github.com/b/pulls")));
  const std::optional<std::string> acme = tree.CreateFolder(*git, 2, u"Acme");
  ASSERT_TRUE(acme);
  EXPECT_EQ((std::vector<std::string>{"Acme CLI", "Acme App", "Acme"}),
            Titles(*tree.Find(*git)));
  const std::optional<Tree::Node> found = tree.Find(*acme);
  ASSERT_TRUE(found);
  EXPECT_EQ(u"Acme", found->title);
  EXPECT_TRUE(found->is_folder);
}

TYPED_TEST(TreeContractTest, AnIndexPastTheEndIsTheEnd) {
  Tree& tree = this->tree();
  tree.CreateFolder("", 0, u"A");
  tree.AddPinned("", 99, u"B", GURL("https://b"));
  EXPECT_EQ((std::vector<std::string>{"A", "B"}), Titles(tree.Root()));
}

TYPED_TEST(TreeContractTest, UnknownParentsAndBadUrlsAreRefused) {
  Tree& tree = this->tree();
  EXPECT_FALSE(tree.CreateFolder("no-such-id", 0, u"X"));
  EXPECT_FALSE(tree.AddPinned("no-such-id", 0, u"X", GURL("https://x")));
  EXPECT_FALSE(tree.AddPinned("", 0, u"X", GURL("not a url")));
  const std::optional<std::string> item =
      tree.AddPinned("", 0, u"I", GURL("https://i"));
  EXPECT_FALSE(tree.CreateFolder(*item, 0, u"Into an item"));
  EXPECT_FALSE(tree.Find("no-such-id"));
}

TYPED_TEST(TreeContractTest, MoveReordersAndReparents) {
  Tree& tree = this->tree();
  const std::optional<std::string> a = tree.CreateFolder("", 0, u"A");
  const std::optional<std::string> b = tree.CreateFolder("", 1, u"B");
  const std::optional<std::string> item =
      tree.AddPinned("", 2, u"Item", GURL("https://i"));
  ASSERT_TRUE(tree.Move(*item, "", 0));
  EXPECT_EQ((std::vector<std::string>{"Item", "A", "B"}), Titles(tree.Root()));
  ASSERT_TRUE(tree.Move(*b, *a, 0));
  EXPECT_EQ(std::vector<std::string>{"B"}, Titles(*tree.Find(*a)));
  EXPECT_EQ((std::vector<std::string>{"Item", "A"}), Titles(tree.Root()));
}

TYPED_TEST(TreeContractTest, AnIndexInTheSameFolderCountsTheItemWhereItIs) {
  Tree& tree = this->tree();
  const std::optional<std::string> a = tree.CreateFolder("", 0, u"A");
  tree.CreateFolder("", 1, u"B");
  tree.CreateFolder("", 2, u"C");
  ASSERT_TRUE(tree.Move(*a, "", 2));
  EXPECT_EQ((std::vector<std::string>{"B", "A", "C"}), Titles(tree.Root()));
  ASSERT_TRUE(tree.Move(*a, "", 99));
  EXPECT_EQ((std::vector<std::string>{"B", "C", "A"}), Titles(tree.Root()));
}

TYPED_TEST(TreeContractTest, WhatChangesNothingIsNotReported) {
  Tree& tree = this->tree();
  const std::optional<std::string> folder = tree.CreateFolder("", 0, u"F");
  const std::optional<std::string> item =
      tree.AddPinned("", 1, u"I", GURL("https://home"));
  tree.SetEssential(*item, true);
  int changes = 0;
  base::CallbackListSubscription subscription = tree.RegisterOnChanged(
      base::BindRepeating([](int* count) { ++*count; }, &changes));
  EXPECT_TRUE(tree.Move(*folder, "", 0));
  EXPECT_TRUE(tree.Move(*folder, "", 1));
  EXPECT_TRUE(tree.Rename(*item, u"I"));
  EXPECT_TRUE(tree.SetHomeUrl(*item, GURL("https://home")));
  EXPECT_TRUE(tree.SetCollapsed(*folder, false));
  EXPECT_TRUE(tree.SetEssential(*item, true));
  EXPECT_EQ(0, changes);
  EXPECT_EQ((std::vector<std::string>{"F", "I"}), Titles(tree.Root()));
}

TYPED_TEST(TreeContractTest, AFolderCannotMoveIntoItselfNorTheRootMove) {
  Tree& tree = this->tree();
  const std::optional<std::string> a = tree.CreateFolder("", 0, u"A");
  const std::optional<std::string> inner = tree.CreateFolder(*a, 0, u"Inner");
  EXPECT_FALSE(tree.Move(*a, *inner, 0));
  EXPECT_FALSE(tree.Move(*a, *a, 0));
  EXPECT_FALSE(tree.Move(tree.Root().id, *a, 0));
  EXPECT_EQ(std::vector<std::string>{"A"}, Titles(tree.Root()));
}

TYPED_TEST(TreeContractTest, FlagsTitlesAndHomeUrlsAreKept) {
  Tree& tree = this->tree();
  const std::optional<std::string> folder = tree.CreateFolder("", 0, u"F");
  const std::optional<std::string> item =
      tree.AddPinned("", 1, u"I", GURL("https://home"));
  EXPECT_FALSE(tree.Find(*folder)->collapsed);
  ASSERT_TRUE(tree.SetCollapsed(*folder, true));
  EXPECT_TRUE(tree.Find(*folder)->collapsed);
  ASSERT_TRUE(tree.SetEssential(*item, true));
  EXPECT_TRUE(tree.Find(*item)->essential);
  ASSERT_TRUE(tree.SetHomeUrl(*item, GURL("https://new-home")));
  EXPECT_EQ(GURL("https://new-home"), tree.Find(*item)->home_url);
  ASSERT_TRUE(tree.Rename(*item, u"Renamed"));
  EXPECT_EQ(u"Renamed", tree.Find(*item)->title);
  EXPECT_FALSE(tree.SetEssential(*folder, true));
  EXPECT_FALSE(tree.SetHomeUrl(*folder, GURL("https://x")));
  EXPECT_FALSE(tree.SetCollapsed(*item, true));
  EXPECT_FALSE(tree.Rename(tree.Root().id, u"Root"));
}

TYPED_TEST(TreeContractTest, EssentialsAreCappedAtTwelve) {
  Tree& tree = this->tree();
  for (int i = 0; i < 12; ++i) {
    const std::optional<std::string> item = tree.AddPinned(
        "", i, u"E", GURL("https://e" + base::NumberToString(i) + ".com"));
    ASSERT_TRUE(tree.SetEssential(*item, true));
  }
  const std::optional<std::string> extra =
      tree.AddPinned("", 12, u"E", GURL("https://x"));
  EXPECT_FALSE(tree.SetEssential(*extra, true));
  EXPECT_EQ(12u, tree.EssentialCount());
}

TYPED_TEST(TreeContractTest, RemoveDeletesTheItemAndWhatIsUnderIt) {
  Tree& tree = this->tree();
  const std::optional<std::string> folder = tree.CreateFolder("", 0, u"F");
  const std::optional<std::string> item =
      tree.AddPinned(*folder, 0, u"I", GURL("https://i"));
  ASSERT_TRUE(tree.Remove(*folder));
  EXPECT_TRUE(tree.Root().children.empty());
  EXPECT_FALSE(tree.Find(*item));
  EXPECT_FALSE(tree.Remove(tree.Root().id));
}

TYPED_TEST(TreeContractTest, ChangesAreReported) {
  Tree& tree = this->tree();
  tree.Root();
  int changes = 0;
  base::CallbackListSubscription subscription = tree.RegisterOnChanged(
      base::BindRepeating([](int* count) { ++*count; }, &changes));
  const std::optional<std::string> folder = tree.CreateFolder("", 0, u"F");
  EXPECT_GT(changes, 0);
  int before = changes;
  tree.Rename(*folder, u"G");
  EXPECT_GT(changes, before);
  before = changes;
  tree.Remove(*folder);
  EXPECT_GT(changes, before);
}

// What is the bookmarks implementation's own.
class BookmarkTreeTest : public testing::Test {
 protected:
  BookmarkTreeTest()
      : model_(bookmarks::TestBookmarkClient::CreateModel()),
        tree_(model_.get()) {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  BookmarkTree tree_;
};

TEST_F(BookmarkTreeTest, TheRootIsAFolderInOtherBookmarksCreatedOnce) {
  const std::string root = tree_.Root().id;
  ASSERT_EQ(1u, model_->other_node()->children().size());
  const bookmarks::BookmarkNode* folder =
      model_->other_node()->children()[0].get();
  EXPECT_TRUE(folder->is_folder());
  EXPECT_EQ(root, folder->uuid().AsLowercaseString());
  EXPECT_EQ(root, tree_.Root().id);
  EXPECT_EQ(1u, model_->other_node()->children().size());
}

TEST_F(BookmarkTreeTest, TheRootIsFoundAgainAfterARenameAndByANewTree) {
  const std::string root = tree_.Root().id;
  model_->SetTitle(model_->other_node()->children()[0].get(), u"Renamed",
                   bookmarks::metrics::BookmarkEditSource::kUser);
  BookmarkTree other(model_.get());
  EXPECT_EQ(root, other.Root().id);
}

TEST_F(BookmarkTreeTest, BookmarksOutsideTheTreeAreNotItems) {
  const bookmarks::BookmarkNode* outside = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"Elsewhere", GURL("https://e"));
  const std::string id = outside->uuid().AsLowercaseString();
  EXPECT_FALSE(tree_.Find(id));
  EXPECT_FALSE(tree_.Remove(id));
  EXPECT_EQ(1u, model_->bookmark_bar_node()->children().size());
}

TEST_F(BookmarkTreeTest, ChangesOutsideTheTreeAreNotReported) {
  tree_.Root();
  int changes = 0;
  base::CallbackListSubscription subscription = tree_.RegisterOnChanged(
      base::BindRepeating([](int* count) { ++*count; }, &changes));
  model_->AddURL(model_->bookmark_bar_node(), 0, u"Elsewhere",
                 GURL("https://e"));
  EXPECT_EQ(0, changes);
}

}  // namespace
}  // namespace cromarc
