// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "cromarc/browser/ports/fake_profiles.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_theme.h"
#include "cromarc/browser/ports/fake_windows.h"
#include "cromarc/browser/spaces/space_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace cromarc {
namespace {

class FakeSidebar : public SpaceController::Sidebar {
 public:
  void OnSpacesChanged() override { ++spaces_changed; }
  void OnSwitchRequested(const std::string& space_id) override {
    switch_requests.push_back(space_id);
  }
  void OnSwipe(const SwipeTracker::Event& event) override {}

  int spaces_changed = 0;
  std::vector<std::string> switch_requests;
};

// Spaces "Default" (Personal) and "Profile 1" (Work), loaded; Personal's
// window on screen with one tab.
class SpaceControllerTest : public testing::Test {
 protected:
  SpaceControllerTest() {
    profiles_.register_prefs = base::BindRepeating(&RegisterSpacePrefs);
    personal_ = profiles_.Add("Default", u"Personal");
    work_ = profiles_.Add("Profile 1", u"Work");
    AddWindow(personal_window_, personal_);
    windows_.bounds[personal_window_] = gfx::Rect(10, 20, 1000, 700);
    tabs_.Add(personal_window_);
    controller_ = std::make_unique<SpaceController>(
        profiles_, windows_, tabs_, theme_, strip_defaults_);
  }

  void AddWindow(BrowserWindowInterface* window, Profile* profile) {
    windows_.windows.push_back(window);
    windows_.profiles[window] = profile;
    windows_.normal_windows[profile].push_back(window);
  }

  std::vector<std::string> SpaceIds() {
    std::vector<std::string> ids;
    for (const SpaceController::Space& space : controller_->GetSpaces()) {
      ids.push_back(space.id);
    }
    return ids;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeProfiles profiles_;
  FakeWindows windows_;
  FakeTabs tabs_;
  FakeTheme theme_;
  FakeStripDefaults strip_defaults_;
  raw_ptr<Profile> personal_ = nullptr;
  raw_ptr<Profile> work_ = nullptr;
  const raw_ptr<BrowserWindowInterface> personal_window_ =
      FakeWindows::Handle<BrowserWindowInterface>(1);
  const raw_ptr<BrowserWindowInterface> work_window_ =
      FakeWindows::Handle<BrowserWindowInterface>(2);
  std::unique_ptr<SpaceController> controller_;
};

TEST_F(SpaceControllerTest, SpacesInTheirOrderThenByName) {
  Profile* zed = profiles_.Add("Profile 2", u"Zed");
  Profile* art = profiles_.Add("Profile 3", u"Art");
  profiles_.Add("Profile 4", u"Asleep", /*loaded=*/false);
  profiles_.prefs(zed).SetInteger(kSpaceOrderPref, 0);
  profiles_.prefs(work_).SetInteger(kSpaceOrderPref, 1);
  EXPECT_EQ((std::vector<std::string>{"Profile 2", "Profile 1", "Profile 3",
                                      "Profile 4", "Default"}),
            SpaceIds());
  EXPECT_FALSE(art == nullptr);
}

TEST_F(SpaceControllerTest, ASwitchSwapsTheWindowsInPlace) {
  windows_.popups = {work_window_};
  controller_->SwitchTo(personal_window_, "Profile 1");
  // Work's new window takes Personal's frame, comes forward with a new tab;
  // Personal's is ordered out.
  EXPECT_EQ(gfx::Rect(10, 20, 1000, 700), windows_.BoundsOf(work_window_));
  EXPECT_TRUE(windows_.IsOnScreen(work_window_));
  EXPECT_EQ(std::vector<BrowserWindowInterface*>{work_window_.get()},
            windows_.activated);
  EXPECT_EQ(1u, tabs_.List(work_window_).size());
  EXPECT_FALSE(windows_.IsOnScreen(personal_window_));
  EXPECT_FALSE(profiles_.prefs(work_).GetTime(kSpaceLastActivePref).is_null());
}

TEST_F(SpaceControllerTest, ASwitchUsesTheSpacesOwnWindow) {
  AddWindow(work_window_, work_);
  windows_.Hide(work_window_);
  controller_->SwitchTo(personal_window_, "Profile 1");
  EXPECT_TRUE(windows_.IsOnScreen(work_window_));
  EXPECT_TRUE(tabs_.List(work_window_).empty());
}

TEST_F(SpaceControllerTest, ASwitchToTheSameSpaceDoesNothing) {
  controller_->SwitchTo(personal_window_, "Default");
  EXPECT_TRUE(windows_.IsOnScreen(personal_window_));
  EXPECT_TRUE(windows_.activated.empty());
}

TEST_F(SpaceControllerTest, AnUnloadedSpaceLoadsFirst) {
  profiles_.Add("Profile 2", u"Later", /*loaded=*/false);
  const raw_ptr<BrowserWindowInterface> later_window =
      FakeWindows::Handle<BrowserWindowInterface>(3);
  windows_.popups = {later_window};
  controller_->SwitchTo(personal_window_, "Profile 2");
  EXPECT_TRUE(windows_.IsOnScreen(later_window));
  EXPECT_FALSE(windows_.IsOnScreen(personal_window_));
}

TEST_F(SpaceControllerTest, OutOfFullscreenTheNewWindowGoesFullscreen) {
  AddWindow(work_window_, work_);
  windows_.Hide(work_window_);
  windows_.fullscreen.insert(personal_window_);
  controller_->SwitchTo(personal_window_, "Profile 1");
  EXPECT_TRUE(windows_.IsFullscreen(work_window_));
  // The fullscreen window keeps its macOS Space; it is not hidden.
  EXPECT_TRUE(windows_.IsOnScreen(personal_window_));
}

TEST_F(SpaceControllerTest, NextAndPreviousGoRoundTheEnd) {
  FakeSidebar sidebar;
  controller_->AddSidebar(personal_window_, &sidebar);
  profiles_.Add("Profile 2", u"Third");
  controller_->RequestSwitchBy(personal_window_, -1);
  controller_->RequestSwitchBy(personal_window_, +1);
  // Personal (Default) is last by name: Personal, Third, Work.
  EXPECT_EQ((std::vector<std::string>{"Profile 1", "Profile 2"}),
            sidebar.switch_requests);
  controller_->RemoveSidebar(personal_window_);
}

TEST_F(SpaceControllerTest, TheLastTabCloseKeepsTheWindow) {
  const TabId last = tabs_.List(personal_window_)[0];
  EXPECT_TRUE(controller_->KeepWindowOnLastTabClose(personal_window_));
  ASSERT_EQ(1u, tabs_.List(personal_window_).size());
  EXPECT_NE(last, tabs_.List(personal_window_)[0]);
}

TEST_F(SpaceControllerTest, OnlyTheLastTabsCloseIsTaken) {
  tabs_.Add(personal_window_);
  EXPECT_FALSE(controller_->KeepWindowOnLastTabClose(personal_window_));
  windows_.not_normal.insert(personal_window_);
  EXPECT_FALSE(controller_->KeepWindowOnLastTabClose(personal_window_));
}

TEST_F(SpaceControllerTest, AfterStartupTheLastActiveSpacesWindowStays) {
  AddWindow(work_window_, work_);
  windows_.bounds[work_window_] = gfx::Rect(50, 60, 800, 600);
  profiles_.prefs(personal_).SetTime(kSpaceLastActivePref,
                                     base::Time::Now() - base::Hours(2));
  profiles_.prefs(work_).SetTime(kSpaceLastActivePref,
                                 base::Time::Now() - base::Hours(1));
  controller_->OnWindowCreated(work_window_);
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(windows_.IsOnScreen(work_window_));
  EXPECT_FALSE(windows_.IsOnScreen(personal_window_));
  // Behind it, in its frame.
  EXPECT_EQ(gfx::Rect(50, 60, 800, 600), windows_.BoundsOf(personal_window_));
}

TEST_F(SpaceControllerTest, OnlyAKnownSpaceGoes) {
  EXPECT_FALSE(controller_->DeleteSpace(personal_window_, "Profile 9"));
  EXPECT_TRUE(profiles_.deleted.empty());
}

TEST_F(SpaceControllerTest, TheLastSpaceStays) {
  FakeProfiles profiles;
  profiles.register_prefs = base::BindRepeating(&RegisterSpacePrefs);
  profiles.Add("Default", u"Only");
  SpaceController controller(profiles, windows_, tabs_, theme_,
                             strip_defaults_);
  EXPECT_FALSE(controller.DeleteSpace(personal_window_, "Default"));
  EXPECT_TRUE(profiles.deleted.empty());
}

TEST_F(SpaceControllerTest, AnotherSpaceIsDeletedAtOnce) {
  EXPECT_TRUE(controller_->DeleteSpace(personal_window_, "Profile 1"));
  EXPECT_EQ(std::vector<std::string>{"Profile 1"}, profiles_.deleted);
  EXPECT_TRUE(windows_.IsOnScreen(personal_window_));
}

TEST_F(SpaceControllerTest, TheShownSpaceIsLeftBeforeItGoes) {
  FakeSidebar sidebar;
  controller_->AddSidebar(personal_window_, &sidebar);
  windows_.popups = {work_window_};
  EXPECT_TRUE(controller_->DeleteSpace(personal_window_, "Default"));
  EXPECT_TRUE(windows_.IsOnScreen(work_window_));
  EXPECT_FALSE(windows_.IsOnScreen(personal_window_));
  EXPECT_EQ(std::vector<std::string>{"Default"}, profiles_.deleted);
  controller_->RemoveSidebar(personal_window_);
}

TEST_F(SpaceControllerTest, ANewSpaceGetsItsIconPlaceAndColour) {
  bool configured = false;
  controller_->CreateSpace(
      personal_window_, u"Third", "🌿",
      base::BindOnce([](bool* configured, Profile*) { *configured = true; },
                     &configured),
      /*switch_to_it=*/false);
  Profile* third = profiles_.HandleOf("Profile 2");
  ASSERT_TRUE(third);
  EXPECT_TRUE(configured);
  EXPECT_EQ("🌿", profiles_.prefs(third).GetString(kSpaceIconPref));
  EXPECT_EQ(3, profiles_.prefs(third).GetInteger(kSpaceOrderPref));
  EXPECT_TRUE(theme_.colors.contains(third));
  // No switch: Personal stays.
  EXPECT_TRUE(windows_.IsOnScreen(personal_window_));
}

TEST_F(SpaceControllerTest, AThemeSetsChromesColourAndScheme) {
  controller_->SetSpaceTheme(
      "Profile 1", R"({"colors": [[10, 20, 30]], "scheme": "dark"})");
  EXPECT_EQ(SkColorSetRGB(10, 20, 30), theme_.colors[work_]);
  EXPECT_EQ(Theme::Scheme::kDark, theme_.schemes[work_]);
  EXPECT_EQ(R"({"colors": [[10, 20, 30]], "scheme": "dark"})",
            profiles_.prefs(work_).GetString(kSpaceThemePref));
}

TEST_F(SpaceControllerTest, ASchemeActsAtOnceAndSavesNothing) {
  controller_->SetSpaceScheme("Profile 1", "dark");
  EXPECT_EQ(Theme::Scheme::kDark, theme_.schemes[work_]);
  controller_->SetSpaceScheme("Profile 1", "auto");
  EXPECT_EQ(Theme::Scheme::kSystem, theme_.schemes[work_]);
  EXPECT_EQ("", profiles_.prefs(work_).GetString(kSpaceThemePref));
  EXPECT_FALSE(theme_.colors.contains(work_));
}

TEST_F(SpaceControllerTest, ARenameOrNewProfileTellsTheSidebars) {
  FakeSidebar sidebar;
  controller_->AddSidebar(personal_window_, &sidebar);
  controller_->RenameSpace("Profile 1", u"Job");
  controller_->RenameSpace("Profile 1", u"");
  EXPECT_EQ(1, sidebar.spaces_changed);
  controller_->RemoveSidebar(personal_window_);
}

}  // namespace
}  // namespace cromarc
