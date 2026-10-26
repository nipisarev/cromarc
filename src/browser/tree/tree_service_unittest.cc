// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/tree/tree_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_tree.h"
#include "cromarc/browser/ports/fake_windows.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

constexpr char kHome[] = "https://github.com/a/pulls";

class TreeServiceTest : public testing::Test {
 protected:
  TreeServiceTest() : service_(std::make_unique<FakeTree>(), tabs_, windows_) {
    item_ = *service_.tree().AddPinned("", 0, u"Acme", GURL(kHome));
  }

  const raw_ptr<BrowserWindowInterface> window_ =
      FakeWindows::Handle<BrowserWindowInterface>(1);
  const raw_ptr<BrowserWindowInterface> other_window_ =
      FakeWindows::Handle<BrowserWindowInterface>(2);
  FakeTabs tabs_;
  FakeWindows windows_;
  TreeService service_;
  std::string item_;
};

TEST_F(TreeServiceTest, AnUnlinkedItemSleeps) {
  EXPECT_FALSE(service_.GetLiveTab(item_));
}

TEST_F(TreeServiceTest, ALinkGoesBothWays) {
  const TabId tab = tabs_.Add(window_);
  service_.Link(item_, tab);
  EXPECT_EQ(tab, service_.GetLiveTab(item_));
  EXPECT_EQ(item_, service_.GetItemForTab(tab));
  EXPECT_FALSE(service_.GetItemForTab(tabs_.Add(window_)));
}

TEST_F(TreeServiceTest, AClosedTabLetsItsItemSleep) {
  const TabId tab = tabs_.Add(window_);
  service_.Link(item_, tab);
  tabs_.Remove(tab);
  EXPECT_FALSE(service_.GetLiveTab(item_));
  EXPECT_FALSE(service_.GetItemForTab(tab));
}

TEST_F(TreeServiceTest, ATabBetweenWindowsKeepsItsLink) {
  const TabId tab = tabs_.Add(window_);
  service_.Link(item_, tab);
  tabs_.Detach(tab);
  EXPECT_EQ(tab, service_.GetLiveTab(item_));
}

TEST_F(TreeServiceTest, OpeningASleepingItemOpensItsHomePinnedAndLinksIt) {
  service_.Open(window_, item_);
  ASSERT_EQ(1u, tabs_.opened.size());
  EXPECT_EQ(window_, tabs_.opened[0].first);
  EXPECT_EQ(GURL(kHome), tabs_.opened[0].second);
  const std::optional<TabId> tab = service_.GetLiveTab(item_);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(tabs_.IsPinned(*tab));
}

TEST_F(TreeServiceTest, OpeningALiveItemFocusesItsTab) {
  const TabId tab = tabs_.Add(window_);
  tabs_.Add(window_);
  tabs_.Activate(tabs_.List(window_)[1]);
  service_.Link(item_, tab);
  service_.Open(window_, item_);
  EXPECT_EQ(tab, tabs_.GetActive(window_));
  EXPECT_TRUE(tabs_.opened.empty());
  EXPECT_TRUE(windows_.activated.empty());
}

TEST_F(TreeServiceTest, OpeningAnItemLiveInAnotherWindowActivatesThatWindow) {
  const TabId tab = tabs_.Add(other_window_);
  service_.Link(item_, tab);
  service_.Open(window_, item_);
  EXPECT_EQ(std::vector<BrowserWindowInterface*>{other_window_},
            windows_.activated);
}

TEST_F(TreeServiceTest, LinksAndBatchesNotifyOnce) {
  int changes = 0;
  base::CallbackListSubscription subscription = service_.RegisterOnChanged(
      base::BindRepeating([](int* changes) { ++*changes; }, &changes));
  service_.Batch([&] {
    service_.Link(item_, tabs_.Add(window_));
    service_.tree().Rename(item_, u"Renamed");
  });
  EXPECT_EQ(1, changes);
  service_.Unlink(item_);
  EXPECT_EQ(2, changes);
  service_.Unlink(item_);
  EXPECT_EQ(2, changes);
}

}  // namespace
}  // namespace cromarc
