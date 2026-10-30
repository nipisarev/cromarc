// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/little/little_window.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cromarc/browser/ports/fake_app_lifecycle.h"
#include "cromarc/browser/ports/fake_presence.h"
#include "cromarc/browser/ports/fake_profiles.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_windows.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

class FakeLittleDelegate : public LittleWindow::Delegate {
 public:
  explicit FakeLittleDelegate(FakeProfiles& profiles) : profiles_(profiles) {}

  std::unique_ptr<views::View> MakeBar(LittleWindow& window) override {
    return nullptr;
  }
  void ShowSpace(
      const std::string& space_id,
      base::OnceCallback<void(BrowserWindowInterface*)> then) override {}
  void LoadSpace(const std::string& space_id,
                 base::OnceCallback<void(Profile*)> then) override {
    std::move(then).Run(profiles_->GetLoaded(space_id));
  }
  void Archive(Profile* profile,
               ArchiveEntry entry,
               bool session_ending) override {
    archived.push_back(std::move(entry));
    archived_at_session_end.push_back(session_ending);
  }

  std::vector<ArchiveEntry> archived;
  std::vector<bool> archived_at_session_end;

 private:
  const raw_ref<FakeProfiles> profiles_;
};

class LittleWindowTest : public testing::Test {
 protected:
  LittleWindowTest() {
    personal_ = profiles_.Add("Default", u"Personal");
    work_ = profiles_.Add("Profile 1", u"Work");
  }

  ~LittleWindowTest() override {
    // Little windows live until their window closes.
    for (BrowserWindowInterface* window : {little_, other_}) {
      if (windows_.FromId(windows_.IdOf(window))) {
        windows_.Close(window);
      }
    }
  }

  // A link from another app, in a Little window of the personal space.
  LittleWindow* OpenLittle() {
    ClosesItsTabs(little_);
    windows_.popups.push_back(little_);
    page_ = tabs_.AddFrom(little_, std::nullopt, page_url_, /*active=*/true);
    LittleWindow::Open(deps_, personal_, page_url_, place_);
    return LittleWindow::From(windows_, little_);
  }

  // As Chrome's: a closing window closes its tabs before it is gone.
  void ClosesItsTabs(BrowserWindowInterface* window) {
    closing_.push_back(windows_.OnClosed(
        window, base::BindRepeating(
                    [](FakeTabs* tabs, BrowserWindowInterface* window) {
                      for (TabId tab : tabs->List(window)) {
                        tabs->Remove(tab);
                      }
                    },
                    &tabs_)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeTabs tabs_;
  FakeWindows windows_;
  FakeProfiles profiles_;
  FakeAppLifecycle app_lifecycle_;
  FakePresence presence_;
  FakeLittleDelegate delegate_{profiles_};
  const LittleWindow::Deps deps_{
      raw_ref<Tabs>(tabs_),           raw_ref<Windows>(windows_),
      raw_ref<Profiles>(profiles_),   raw_ref<AppLifecycle>(app_lifecycle_),
      raw_ref<Presence>(presence_),   raw_ref<LittleWindow::Delegate>(delegate_)};
  raw_ptr<Profile> personal_ = nullptr;
  raw_ptr<Profile> work_ = nullptr;
  const raw_ptr<BrowserWindowInterface> little_ =
      FakeWindows::Handle<BrowserWindowInterface>(1);
  const raw_ptr<BrowserWindowInterface> other_ =
      FakeWindows::Handle<BrowserWindowInterface>(2);
  const GURL page_url_{"https://doc.test/read"};
  const gfx::Rect place_{40, 60, 900, 650};
  TabId page_{0};
  std::vector<base::CallbackListSubscription> closing_;
};

TEST_F(LittleWindowTest, ClosingItArchivesItsPage) {
  OpenLittle()->Close();
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_EQ(delegate_.archived[0].url, page_url_);
  EXPECT_EQ(delegate_.archived[0].reason, "little");
  EXPECT_FALSE(delegate_.archived_at_session_end[0]);
}

TEST_F(LittleWindowTest, MoveToSpaceOpensThePageThereInThisOnesPlace) {
  LittleWindow* little = OpenLittle();
  EXPECT_EQ(little->default_space(), "Default");
  windows_.popups.push_back(other_);
  little->MoveToSpace("Profile 1");

  ASSERT_EQ(windows_.popup_urls.size(), 2u);
  EXPECT_EQ(windows_.popup_urls[1], page_url_);
  EXPECT_EQ(windows_.ProfileOf(other_), work_.get());
  EXPECT_EQ(windows_.BoundsOf(other_), place_);
  // A Little window there, with its bar.
  ASSERT_TRUE(LittleWindow::From(windows_, other_));
  EXPECT_EQ(LittleWindow::From(windows_, other_)->default_space(),
            "Profile 1");
  EXPECT_EQ(windows_.toolbar_views[other_], 1);
  // This one is gone, its page not in the archive: it lives on there.
  EXPECT_EQ(windows_.closed, std::vector<BrowserWindowInterface*>{little_});
  EXPECT_FALSE(LittleWindow::From(windows_, little_));
  EXPECT_TRUE(delegate_.archived.empty());
}

TEST_F(LittleWindowTest, MovingToItsOwnSpaceDoesNothing) {
  LittleWindow* little = OpenLittle();
  windows_.popups.push_back(other_);
  little->MoveToSpace("Default");
  EXPECT_EQ(windows_.popup_urls.size(), 1u);
  EXPECT_TRUE(windows_.closed.empty());
}

TEST_F(LittleWindowTest, MovingToASpaceThatDoesNotLoadKeepsTheWindow) {
  LittleWindow* little = OpenLittle();
  windows_.popups.push_back(other_);
  little->MoveToSpace("Profile 9");
  EXPECT_EQ(windows_.popup_urls.size(), 1u);
  EXPECT_TRUE(windows_.closed.empty());
  EXPECT_TRUE(delegate_.archived.empty());
}

TEST_F(LittleWindowTest, UnusedForSixHoursItClosesIntoTheArchive) {
  OpenLittle();
  task_environment_.FastForwardBy(base::Hours(5) + base::Minutes(50));
  EXPECT_TRUE(windows_.closed.empty());
  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_EQ(windows_.closed, std::vector<BrowserWindowInterface*>{little_});
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_EQ(delegate_.archived[0].url, page_url_);
}

TEST_F(LittleWindowTest, InFrontWithTheUserThereItStays) {
  OpenLittle();
  windows_.active.insert(little_);
  task_environment_.FastForwardBy(base::Hours(7));
  EXPECT_TRUE(windows_.closed.empty());

  // Away from the Mac, the window in front is unused too.
  presence_.idle_for = base::Hours(1);
  task_environment_.FastForwardBy(base::Hours(6) + base::Minutes(5));
  EXPECT_EQ(windows_.closed, std::vector<BrowserWindowInterface*>{little_});
}

TEST_F(LittleWindowTest, UnsavedInputKeepsIt) {
  OpenLittle();
  tabs_.may_lose_input.insert(page_);
  task_environment_.FastForwardBy(base::Hours(7));
  EXPECT_TRUE(windows_.closed.empty());
  EXPECT_TRUE(delegate_.archived.empty());
}

TEST_F(LittleWindowTest, AtSessionEndItsPageGoesToTheArchive) {
  OpenLittle();
  app_lifecycle_.session_ending = true;
  app_lifecycle_.CloseAllWindows(true);
  ASSERT_EQ(delegate_.archived.size(), 1u);
  EXPECT_TRUE(delegate_.archived_at_session_end[0]);
  // Its tabs closing then do not archive it twice.
  windows_.Close(little_);
  EXPECT_EQ(delegate_.archived.size(), 1u);
}

}  // namespace
}  // namespace cromarc
