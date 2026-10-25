// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/archive/archive.h"

#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

class ArchiveTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    archive_ = std::make_unique<Archive>(dir_.GetPath().AppendASCII("Archive"));
    ASSERT_TRUE(archive_->Init());
  }

  ArchiveEntry Entry(const std::string& url,
                     const std::string& title,
                     base::Time closed_at) {
    ArchiveEntry entry;
    entry.url = GURL(url);
    entry.title = base::UTF8ToUTF16(title);
    entry.closed_at = closed_at;
    entry.reason = "idle";
    entry.folder_path = "";
    return entry;
  }

  std::vector<std::string> Titles(const std::vector<ArchiveEntry>& entries) {
    std::vector<std::string> titles;
    for (const auto& e : entries) {
      titles.push_back(base::UTF16ToUTF8(e.title));
    }
    return titles;
  }

  const base::Time now_ = base::Time::Now();
  base::ScopedTempDir dir_;
  std::unique_ptr<Archive> archive_;
};

// Firefox keeps no closed blank tabs; a blank page or a New Tab says nothing
// about what was being done.
TEST_F(ArchiveTest, BlankPagesAreNotKept) {
  EXPECT_FALSE(archive_->Add(Entry("about:blank", "about:blank", now_)));
  EXPECT_FALSE(archive_->Add(Entry("chrome://newtab/", "New Tab", now_)));
  EXPECT_FALSE(
      archive_->Add(Entry("chrome://new-tab-page/", "New Tab", now_)));
  EXPECT_TRUE(archive_->Add(Entry("https://a.test/", "A", now_)));
  EXPECT_EQ(Titles(archive_->Search(u"", 10)),
            std::vector<std::string>({"A"}));
}

TEST_F(ArchiveTest, EmptyQueryListsNewestFirst) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "A", now_ - base::Hours(2))));
  ASSERT_TRUE(archive_->Add(Entry("https://b", "B", now_ - base::Hours(1))));
  EXPECT_EQ(Titles(archive_->Search(u"", 10)),
            (std::vector<std::string>{"B", "A"}));
}

TEST_F(ArchiveTest, EveryWordMustMatchTitleOrUrlCaseInsensitively) {
  ASSERT_TRUE(archive_->Add(Entry("https://github.com/acme/app/pulls",
                                  "Pull requests", now_)));
  ASSERT_TRUE(archive_->Add(Entry("https://docs.google.com/x",
                                  "Черновик договора", now_)));
  EXPECT_EQ(Titles(archive_->Search(u"acme PULL", 10)),
            (std::vector<std::string>{"Pull requests"}));
  EXPECT_EQ(Titles(archive_->Search(u"черновик", 10)),
            (std::vector<std::string>{"Черновик договора"}));
  EXPECT_TRUE(archive_->Search(u"acme docs", 10).empty());
}

TEST_F(ArchiveTest, WildcardsInTheQueryAreLiteral) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "100% done", now_)));
  ASSERT_TRUE(archive_->Add(Entry("https://b", "1000 done", now_)));
  EXPECT_EQ(Titles(archive_->Search(u"0%", 10)),
            (std::vector<std::string>{"100% done"}));
}

TEST_F(ArchiveTest, TheSameUrlUpdatesItsRowAndCounts) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "Old", now_ - base::Days(3))));
  ASSERT_TRUE(archive_->Add(Entry("https://a", "New", now_)));
  std::vector<ArchiveEntry> all = archive_->Search(u"", 10);
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].title, u"New");
  EXPECT_EQ(all[0].count, 2);
}

TEST_F(ArchiveTest, ExpireDropsOldRowsAndKeepsTheNewestUnderTheCap) {
  ASSERT_TRUE(archive_->Add(Entry("https://old", "Old", now_ - base::Days(91))));
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(archive_->Add(Entry("https://n" + base::NumberToString(i),
                                    "N" + base::NumberToString(i),
                                    now_ - base::Minutes(10 - i))));
  }
  archive_->Expire(now_, /*max_age=*/base::Days(90), /*max_rows=*/3);
  EXPECT_EQ(Titles(archive_->Search(u"", 10)),
            (std::vector<std::string>{"N4", "N3", "N2"}));
}

TEST_F(ArchiveTest, RemoveDeletesOneRow) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "A", now_)));
  ASSERT_TRUE(archive_->Add(Entry("https://b", "B", now_)));
  const int64_t id = archive_->Search(u"A", 10)[0].id;
  ASSERT_TRUE(archive_->Remove(id));
  EXPECT_EQ(Titles(archive_->Search(u"", 10)), (std::vector<std::string>{"B"}));
}

TEST_F(ArchiveTest, RemoveUrlDeletesThatUrlsRow) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "A", now_)));
  ASSERT_TRUE(archive_->Add(Entry("https://b", "B", now_)));
  ASSERT_TRUE(archive_->RemoveUrl(GURL("https://a")));
  EXPECT_FALSE(archive_->RemoveUrl(GURL("https://a")));
  EXPECT_EQ(Titles(archive_->Search(u"", 10)), (std::vector<std::string>{"B"}));
}

TEST_F(ArchiveTest, RowsSurviveReopening) {
  ASSERT_TRUE(archive_->Add(Entry("https://a", "A", now_)));
  archive_.reset();
  Archive reopened(dir_.GetPath().AppendASCII("Archive"));
  ASSERT_TRUE(reopened.Init());
  EXPECT_EQ(Titles(reopened.Search(u"", 10)), (std::vector<std::string>{"A"}));
}

}  // namespace
}  // namespace cromarc
