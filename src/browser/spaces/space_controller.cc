// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_controller.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: as above
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "cromarc/browser/spaces/space_prefs.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// Session restore brings windows back during the first seconds; only then are
// inactive spaces' windows hidden.
constexpr base::TimeDelta kStartupWindow = base::Seconds(20);
constexpr base::TimeDelta kHideDelay = base::Milliseconds(1500);
// After startup settles, in idle time.
constexpr base::TimeDelta kWarmWindowsDelay = base::Seconds(30);

ProfileAttributesStorage& Storage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

std::string FirstLetter(const std::u16string& name) {
  base::i18n::UTF16CharIterator it(name);
  if (it.end()) {
    return "?";
  }
  std::u16string letter;
  base::WriteUnicodeCharacter(it.get(), &letter);
  return base::UTF16ToUTF8(letter);
}

bool IsVisible(BrowserWindowInterface* browser) {
  const ui::BaseWindow* window = browser->GetWindow();
  return window && window->IsVisible() && !window->IsMinimized();
}

}  // namespace

// static
SpaceController* SpaceController::Get() {
  static base::NoDestructor<SpaceController> instance;
  return instance.get();
}

SpaceController::SpaceController() {
  storage_observation_.Observe(&Storage());
}

SpaceController::~SpaceController() = default;

// static
std::string SpaceController::IdOf(Profile* profile) {
  return profile->GetOriginalProfile()->GetBaseName().AsUTF8Unsafe();
}

std::vector<SpaceController::Space> SpaceController::GetSpaces() {
  struct Ordered {
    Space space;
    int order;
  };
  std::vector<Ordered> spaces;
  ProfileManager* manager = g_browser_process->profile_manager();
  for (ProfileAttributesEntry* entry : Storage().GetAllProfilesAttributes()) {
    if (entry->IsOmitted() || entry->IsEphemeral()) {
      continue;
    }
    Space space;
    space.path = entry->GetPath();
    space.id = space.path.BaseName().AsUTF8Unsafe();
    space.name = entry->GetLocalProfileName();
    space.color = entry->GetProfileThemeColors().profile_highlight_color;
    int order = -1;
    if (Profile* profile = manager->GetProfileByPath(space.path)) {
      space.icon = profile->GetPrefs()->GetString(kSpaceIconPref);
      order = profile->GetPrefs()->GetInteger(kSpaceOrderPref);
    }
    if (space.icon.empty()) {
      space.icon = FirstLetter(space.name);
    }
    spaces.push_back({std::move(space), order});
  }
  std::ranges::sort(spaces, [](const Ordered& a, const Ordered& b) {
    // Ordered spaces first, then the rest by name.
    if ((a.order < 0) != (b.order < 0)) {
      return a.order >= 0;
    }
    if (a.order != b.order) {
      return a.order < b.order;
    }
    return a.space.name < b.space.name;
  });
  std::vector<Space> result;
  for (Ordered& o : spaces) {
    result.push_back(std::move(o.space));
  }
  return result;
}

Profile* SpaceController::GetLoadedProfile(const std::string& space_id) {
  return g_browser_process->profile_manager()->GetProfileByPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(
          space_id));
}

// static
BrowserWindowInterface* SpaceController::GetWindowFor(Profile* profile) {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile &&
            browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
          found = browser;
          return false;
        }
        return true;
      });
  return found;
}

void SpaceController::AddSidebar(BrowserWindowInterface* browser,
                                 Sidebar* sidebar) {
  sidebars_[browser] = sidebar;
  close_subscriptions_[browser] = browser->RegisterBrowserDidClose(
      base::BindRepeating(&SpaceController::OnSpaceWindowClosed,
                          base::Unretained(this)));
}

void SpaceController::RemoveSidebar(BrowserWindowInterface* browser) {
  sidebars_.erase(browser);
}

void SpaceController::OnSpaceWindowClosed(BrowserWindowInterface* browser) {
  close_subscriptions_.erase(browser);
  if (!IsVisible(browser)) {
    return;
  }
  for (BrowserWindowInterface* other : GetAllBrowserWindowInterfaces()) {
    if (other != browser &&
        other->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
        !other->GetProfile()->IsOffTheRecord() && !IsVisible(other)) {
      other->GetWindow()->Close();
    }
  }
}

bool SpaceController::KeepWindowOnLastTabClose(
    BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      browser->GetProfile()->IsOffTheRecord()) {
    return false;
  }
  TabStripModel* model = browser->GetTabStripModel();
  if (model->count() != 1) {
    return false;
  }
  tabs::TabInterface* last = model->GetTabAtIndex(0);
  chrome::AddTabAt(browser, GURL(chrome::kChromeUINewTabURL), -1,
                   /*foreground=*/true);
  model->CloseWebContentsAt(model->GetIndexOfTab(last),
                            TabCloseTypes::CLOSE_USER_GESTURE |
                                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  return true;
}

void SpaceController::PrepareWarmWindows() {
  ProfileManager* manager = g_browser_process->profile_manager();
  for (const Space& space : GetSpaces()) {
    if (Profile* profile = manager->GetProfileByPath(space.path)) {
      CreateHiddenWindow(profile);
      continue;
    }
    manager->LoadProfileByPath(
        space.path, /*incognito=*/false,
        base::BindOnce(
            [](base::WeakPtr<SpaceController> self, Profile* profile) {
              if (self && profile) {
                self->CreateHiddenWindow(profile);
              }
            },
            weak_factory_.GetWeakPtr()));
  }
}

void SpaceController::CreateHiddenWindow(Profile* profile) {
  if (GetWindowFor(profile)) {
    return;
  }
  // Same frame as the visible space window, so the swap shows no jump.
  gfx::Rect bounds;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
        IsVisible(browser)) {
      bounds = browser->GetWindow()->GetBounds();
      break;
    }
  }
  BrowserWindowCreateParams params(*profile, /*from_user_gesture=*/false);
  params.initial_bounds = bounds;
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return;
  }
  chrome::AddTabAt(browser, GURL(chrome::kChromeUINewTabURL), -1,
                   /*foreground=*/true, /*group=*/std::nullopt,
                   /*pinned=*/false, NavigateParams::WindowAction::kNoAction);
}

void SpaceController::RequestSwitchTo(BrowserWindowInterface* from,
                                      const std::string& id) {
  auto it = sidebars_.find(from);
  if (it != sidebars_.end()) {
    it->second->OnSwitchRequested(id);
  } else {
    SwitchTo(from, id);
  }
}

void SpaceController::RequestSwitchToIndex(BrowserWindowInterface* from,
                                           size_t index) {
  std::vector<Space> spaces = GetSpaces();
  if (index < spaces.size()) {
    RequestSwitchTo(from, spaces[index].id);
  }
}

void SpaceController::RequestSwitchBy(BrowserWindowInterface* from, int step) {
  std::vector<Space> spaces = GetSpaces();
  if (spaces.size() < 2) {
    return;
  }
  const std::string current = IdOf(from->GetProfile());
  auto it = std::ranges::find(spaces, current, &Space::id);
  const int count = static_cast<int>(spaces.size());
  const int index = it == spaces.end() ? 0 : it - spaces.begin();
  RequestSwitchTo(from, spaces[(index + step % count + count) % count].id);
}

void SpaceController::SwitchTo(BrowserWindowInterface* from,
                               const std::string& id) {
  if (IdOf(from->GetProfile()) == id) {
    return;
  }
  if (Profile* target = GetLoadedProfile(id)) {
    SwitchToLoaded(from->GetSessionID(), target);
    return;
  }
  // Cold: the space's profile loads first.
  const SessionID from_id = from->GetSessionID();
  g_browser_process->profile_manager()->LoadProfileByPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(id),
      /*incognito=*/false,
      base::BindOnce(
          [](base::WeakPtr<SpaceController> self, SessionID from_id,
             Profile* profile) {
            if (self && profile) {
              self->SwitchToLoaded(from_id, profile);
            }
          },
          weak_factory_.GetWeakPtr(), from_id));
}

void SpaceController::SwitchToLoaded(SessionID from_id, Profile* target) {
  BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id);
  if (!from) {
    return;
  }
  const gfx::Rect bounds = from->GetWindow()->GetBounds();
  BrowserWindowInterface* to = GetWindowFor(target);
  if (!to) {
    BrowserWindowCreateParams params(*target, /*from_user_gesture=*/true);
    params.initial_bounds = bounds;
    to = CreateBrowserWindow(std::move(params));
    if (!to) {
      return;
    }
    to->OpenGURL(GURL(chrome::kChromeUINewTabURL),
                 WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
  to->GetWindow()->SetBounds(bounds);
  to->GetWindow()->Show();
  to->GetWindow()->Activate();
  from->GetWindow()->Hide();
  target->GetPrefs()->SetTime(kSpaceLastActivePref, base::Time::Now());
}

void SpaceController::OnSwipe(BrowserWindowInterface* browser,
                              const SwipeTracker::Event& event) {
  auto it = sidebars_.find(browser);
  if (it != sidebars_.end()) {
    it->second->OnSwipe(event);
  }
}

void SpaceController::OnWindowCreated(BrowserWindowInterface* browser) {
  if (!warm_windows_scheduled_ && !browser->GetProfile()->IsOffTheRecord()) {
    warm_windows_scheduled_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpaceController::PrepareWarmWindows,
                       weak_factory_.GetWeakPtr()),
        kWarmWindowsDelay);
  }
  if (base::TimeTicks::Now() - started_ > kStartupWindow) {
    browser->GetProfile()->GetPrefs()->SetTime(kSpaceLastActivePref,
                                               base::Time::Now());
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpaceController::HideInactiveSpaceWindows,
                     weak_factory_.GetWeakPtr()),
      kHideDelay);
}

void SpaceController::HideInactiveSpaceWindows() {
  // The visible space windows; the one whose space was active last stays.
  std::vector<BrowserWindowInterface*> visible;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
        !browser->GetProfile()->IsOffTheRecord() && IsVisible(browser)) {
      visible.push_back(browser);
    }
  }
  if (visible.size() < 2) {
    return;
  }
  BrowserWindowInterface* keep = *std::ranges::max_element(
      visible, {}, [](BrowserWindowInterface* browser) {
        return browser->GetProfile()->GetPrefs()->GetTime(
            kSpaceLastActivePref);
      });
  const gfx::Rect bounds = keep->GetWindow()->GetBounds();
  for (BrowserWindowInterface* browser : visible) {
    if (browser->GetProfile() != keep->GetProfile()) {
      browser->GetWindow()->SetBounds(bounds);
      browser->GetWindow()->Hide();
    }
  }
}

void SpaceController::NotifySpacesChanged() {
  for (auto& [browser, sidebar] : sidebars_) {
    sidebar->OnSpacesChanged();
  }
}

void SpaceController::OnProfileAdded(const base::FilePath& profile_path) {
  NotifySpacesChanged();
}

void SpaceController::OnProfileWasRemoved(const base::FilePath& profile_path,
                                          const std::u16string& profile_name) {
  NotifySpacesChanged();
}

void SpaceController::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  NotifySpacesChanged();
}

}  // namespace cromarc
