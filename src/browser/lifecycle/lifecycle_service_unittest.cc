// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/lifecycle/lifecycle_service.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/ports/fake_app_lifecycle.h"
#include "cromarc/browser/ports/fake_glance_surface.h"
#include "cromarc/browser/ports/fake_presence.h"
#include "cromarc/browser/ports/fake_tabs.h"
#include "cromarc/browser/ports/fake_tree.h"
#include "cromarc/browser/ports/fake_windows.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/glance/glance_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cromarc {
namespace {

class FakeGlanceDelegate : public GlanceRegistry::Delegate {
 public:
  TabKind KindOf(TabId tab) override {
    auto it = kinds.find(tab);
    return it == kinds.end() ? TabKind::kToday : it->second;
  }
  void Archive(TabId glance, bool session_ending) override {
    archived.push_back(glance);
  }

  std::map<TabId, TabKind> kinds;
  std::vector<TabId> archived;
};

// A glance lives and dies with its tab: the lifecycle never times a glance
// out itself, and its tab is in use while it shows.
class LifecycleServiceGlanceTest : public testing::Test {
 protected:
  LifecycleServiceGlanceTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    prefs_.registry()->RegisterListPref(kArchivePendingPref);
    archive_ =
        std::make_unique<ArchiveService>(temp_dir_.GetPath(), &prefs_);
    auto tree = std::make_unique<FakeTree>();
    tree_ = tree.get();
    tree_service_ =
        std::make_unique<TreeService>(std::move(tree), tabs_, windows_);

    windows_.windows.push_back(window_);
    windows_.profiles[window_] = profile_;
    windows_.normal_windows[profile_] = {window_};
    today_ = tabs_.AddFrom(window_, std::nullopt, GURL("https://a.test/"),
                           /*active=*/true);
    essential_ = AddEssential();
    glances_.Watch(window_);

    service_ = std::make_unique<LifecycleService>(
        profile_, tree_service_.get(), archive_.get(), tabs_, windows_,
        app_lifecycle_, presence_, glances_);
  }

  ~LifecycleServiceGlanceTest() override {
    service_->Shutdown();
    service_.reset();
    archive_.reset();
    task_environment_.RunUntilIdle();
  }

  TabId AddEssential() {
    const GURL home("https://essential.test/");
    const TabId tab = tabs_.AddFrom(window_, std::nullopt, home,
                                    /*active=*/false);
    tabs_.SetPinned(tab, true);
    const std::optional<std::string> item =
        tree_->AddPinned(tree_->Root().id, 0, u"Essential", home);
    CHECK(item);
    tree_->SetEssential(*item, true);
    tree_service_->Link(*item, tab);
    delegate_.kinds[tab] = TabKind::kEssential;
    return tab;
  }

  // Option+click on a link of `parent`: its glance, shown.
  TabId Glance(TabId parent) {
    tabs_.Activate(parent);
    EXPECT_TRUE(glances_.Expect(parent, std::nullopt));
    tabs_.OpenFrom(parent, content::OpenURLParams(
                               GURL("https://b.test/doc"), content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/true));
    const std::optional<TabId> glance = glances_.GlanceOf(parent);
    EXPECT_TRUE(glance);
    return glance.value_or(TabId(0));
  }

  // Longer than the idle limit, sweeping as the service does.
  void WaitOutIdleLimit() { task_environment_.FastForwardBy(base::Hours(13)); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  const raw_ptr<Profile> profile_ = FakeWindows::Handle<Profile>(7);
  const raw_ptr<BrowserWindowInterface> window_ =
      FakeWindows::Handle<BrowserWindowInterface>(1);
  FakeTabs tabs_;
  FakeWindows windows_;
  FakeAppLifecycle app_lifecycle_;
  FakePresence presence_;
  FakeGlanceSurface surface_;
  FakeGlanceDelegate delegate_;
  GlanceRegistry glances_{tabs_, windows_, app_lifecycle_, surface_,
                          delegate_};
  std::unique_ptr<ArchiveService> archive_;
  raw_ptr<FakeTree> tree_ = nullptr;
  std::unique_ptr<TreeService> tree_service_;
  std::unique_ptr<LifecycleService> service_;
  TabId today_{0};
  TabId essential_{0};
};

TEST_F(LifecycleServiceGlanceTest, AnEssentialWhoseGlanceShowsIsInUse) {
  // Playing sound, the Today tab stays too: the Essential is not the
  // window's last page.
  tabs_.audible.insert(today_);
  const TabId glance = Glance(essential_);
  WaitOutIdleLimit();
  EXPECT_TRUE(tabs_.Exists(essential_));
  EXPECT_TRUE(tabs_.Exists(glance));
  EXPECT_TRUE(tabs_.Exists(today_));
}

TEST_F(LifecycleServiceGlanceTest,
       AwayFromItTheEssentialSleepsAndItsGlanceGoesToTheArchive) {
  const TabId glance = Glance(essential_);
  tabs_.Activate(today_);
  WaitOutIdleLimit();
  EXPECT_FALSE(tabs_.Exists(essential_));
  EXPECT_FALSE(tabs_.Exists(glance));
  // Archived as a glance, with its tab, not timed out as a Today tab.
  EXPECT_EQ(delegate_.archived, std::vector<TabId>{glance});
  EXPECT_TRUE(tabs_.Exists(today_));
}

TEST_F(LifecycleServiceGlanceTest,
       AWindowWithAnEssentialAndItsGlanceOnlyKeepsThem) {
  tabs_.Remove(today_);
  const TabId glance = Glance(essential_);
  windows_.off_screen.insert(window_);
  WaitOutIdleLimit();
  // The glance is no page of the window's: the Essential is its last one.
  EXPECT_TRUE(tabs_.Exists(essential_));
  EXPECT_TRUE(tabs_.Exists(glance));
}

TEST_F(LifecycleServiceGlanceTest, UnsavedInputInTheGlanceDefersItsTab) {
  const TabId glance = Glance(essential_);
  tabs_.Activate(today_);
  tabs_.may_lose_input.insert(glance);
  WaitOutIdleLimit();
  EXPECT_TRUE(tabs_.Exists(essential_));
  EXPECT_TRUE(tabs_.Exists(glance));
  const std::vector<LifecycleService::TabState> states =
      service_->GetTabStates(window_);
  ASSERT_EQ(states.size(), 1u);
  EXPECT_EQ(states[0].tab, essential_);
  EXPECT_TRUE(states[0].deferred);
}

TEST_F(LifecycleServiceGlanceTest, AGlanceShowsNoCountdownOfItsOwn) {
  const TabId glance = Glance(essential_);
  tabs_.Activate(today_);
  task_environment_.FastForwardBy(base::Hours(11) + base::Minutes(30));
  for (const LifecycleService::TabState& state :
       service_->GetTabStates(window_)) {
    EXPECT_NE(state.tab, glance);
  }
}

TEST_F(LifecycleServiceGlanceTest, ClearLeavesAGlanceToItsTab) {
  const TabId glance = Glance(essential_);
  tabs_.Activate(today_);
  EXPECT_EQ(service_->ClearToday(window_),
            std::vector<GURL>{GURL("https://a.test/")});
  EXPECT_FALSE(tabs_.Exists(today_));
  EXPECT_TRUE(tabs_.Exists(glance));
  EXPECT_TRUE(tabs_.Exists(essential_));
}

}  // namespace
}  // namespace cromarc
