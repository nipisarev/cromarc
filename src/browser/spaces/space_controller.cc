// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_controller.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/browser_commands.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_tabstrip.h"  // nogncheck: as above
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: as above
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"  // nogncheck: as above
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "cromarc/browser/spaces/space_prefs.h"
#include "cromarc/browser/spaces/space_window.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace cromarc {

namespace {

// New spaces' colours, in turn: distinct hues until the theme editor.
constexpr auto kNewSpaceColors = std::to_array<SkColor>({
    SkColorSetRGB(0x6B, 0x40, 0xD9), SkColorSetRGB(0x1F, 0x9E, 0x8F),
    SkColorSetRGB(0xE0, 0x7A, 0x2E), SkColorSetRGB(0xD9, 0x40, 0x7A),
    SkColorSetRGB(0x3B, 0x82, 0xF6), SkColorSetRGB(0x5C, 0xA8, 0x3D),
});

// Session restore brings windows back during the first seconds; only then are
// inactive spaces' windows hidden.
constexpr base::TimeDelta kStartupWindow = base::Seconds(20);
constexpr base::TimeDelta kHideDelay = base::Milliseconds(1500);
// After startup settles, in idle time.
constexpr base::TimeDelta kWarmWindowsDelay = base::Seconds(30);

ProfileAttributesStorage& Storage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

bool IsVisible(BrowserWindowInterface* browser) {
  const ui::BaseWindow* window = browser->GetWindow();
  return window && window->IsVisible() && !window->IsMinimized();
}

bool IsFullscreen(BrowserWindowInterface* browser) {
  return browser && browser->GetWindow()->IsFullscreen();
}

// The frame a space window takes: a fullscreen window's is the one it leaves
// fullscreen to.
gfx::Rect FrameOf(BrowserWindowInterface* browser) {
  return IsFullscreen(browser) ? browser->GetWindow()->GetRestoredBounds()
                               : browser->GetWindow()->GetBounds();
}

// The parts of a space theme Chrome's own theme takes.
void ApplyTheme(Profile* profile, const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json, 0);
  const base::DictValue* theme = value ? value->GetIfDict() : nullptr;
  if (!theme) {
    return;
  }
  ThemeService* service = ThemeServiceFactory::GetForProfile(profile);
  const base::ListValue* colors = theme->FindList("colors");
  if (colors && !colors->empty() && (*colors)[0].is_list() &&
      (*colors)[0].GetList().size() == 3) {
    const base::ListValue& rgb = (*colors)[0].GetList();
    service->SetUserColorAndBrowserColorVariant(
        SkColorSetRGB(rgb[0].GetIfInt().value_or(0),
                      rgb[1].GetIfInt().value_or(0),
                      rgb[2].GetIfInt().value_or(0)),
        ui::mojom::BrowserColorVariant::kTonalSpot);
  }
  const std::string* scheme = theme->FindString("scheme");
  service->SetBrowserColorScheme(
      !scheme                ? ThemeService::BrowserColorScheme::kSystem
      : *scheme == "light"   ? ThemeService::BrowserColorScheme::kLight
      : *scheme == "dark"    ? ThemeService::BrowserColorScheme::kDark
                             : ThemeService::BrowserColorScheme::kSystem);
}

}  // namespace

// static
SpaceController* SpaceController::Get() {
  static base::NoDestructor<SpaceController> instance;
  return instance.get();
}

SpaceController::SpaceController() {
  storage_observation_.Observe(&Storage());
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
}

SpaceController::~SpaceController() = default;

void SpaceController::OnProfileManagerDestroying() {
  storage_observation_.Reset();
  profile_manager_observation_.Reset();
}

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
      space.theme = profile->GetPrefs()->GetString(kSpaceThemePref);
      order = profile->GetPrefs()->GetInteger(kSpaceOrderPref);
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
  auto* controller = tabs::VerticalTabStripStateController::From(browser);
  if (!controller) {
    return;
  }
  std::vector<base::CallbackListSubscription>& subscriptions =
      sidebar_state_subscriptions_[browser];
  subscriptions.clear();
  subscriptions.push_back(controller->RegisterOnCollapseChanged(
      base::BindRepeating(
          [](SpaceController* self, BrowserWindowInterface* browser,
             tabs::VerticalTabStripCollapseState) {
            self->ShareSidebarState(browser);
          },
          base::Unretained(this), browser)));
  // A width the user drags is shared once the drag ends.
  subscriptions.push_back(controller->RegisterOnResizingChanged(
      base::BindRepeating(
          [](SpaceController* self, BrowserWindowInterface* browser,
             bool resizing) {
            if (!resizing) {
              self->ShareSidebarState(browser);
            }
          },
          base::Unretained(this), browser)));
  if (sidebar_state_) {
    base::AutoReset<bool> sharing(&sharing_sidebar_state_, true);
    ApplySidebarState(browser);
  } else {
    ShareSidebarState(browser);
  }
}

void SpaceController::RemoveSidebar(BrowserWindowInterface* browser) {
  sidebars_.erase(browser);
  sidebar_state_subscriptions_.erase(browser);
}

void SpaceController::ShareSidebarState(BrowserWindowInterface* from) {
  auto* controller = tabs::VerticalTabStripStateController::From(from);
  if (sharing_sidebar_state_ || !controller || controller->IsResizing()) {
    return;
  }
  // Collapsing counts as collapsed: that is where it is going.
  const SidebarState state{
      .collapsed = controller->GetCollapseState() !=
                   tabs::VerticalTabStripCollapseState::kExpanded,
      .width = controller->GetUncollapsedWidth()};
  if (sidebar_state_ == state) {
    return;
  }
  sidebar_state_ = state;
  base::AutoReset<bool> sharing(&sharing_sidebar_state_, true);
  for (const auto& [browser, sidebar] : sidebars_) {
    if (browser != from) {
      ApplySidebarState(browser);
    }
  }
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    SeedSidebarState(profile);
  }
}

void SpaceController::ApplySidebarState(BrowserWindowInterface* browser) {
  auto* controller = tabs::VerticalTabStripStateController::From(browser);
  if (!controller || !sidebar_state_) {
    return;
  }
  if (controller->GetUncollapsedWidth() != sidebar_state_->width) {
    controller->SetUncollapsedWidth(sidebar_state_->width);
  }
  const bool collapsed = controller->GetCollapseState() !=
                         tabs::VerticalTabStripCollapseState::kExpanded;
  if (collapsed != sidebar_state_->collapsed) {
    controller->RequestCollapse(sidebar_state_->collapsed);
  }
}

void SpaceController::SeedSidebarState(Profile* profile) {
  if (!sidebar_state_ || profile->IsOffTheRecord()) {
    return;
  }
  // What Chrome gives a window that session restore does not bring back.
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kVerticalTabsCollapsedState,
                    sidebar_state_->collapsed);
  prefs->SetInteger(prefs::kVerticalTabsUncollapsedWidth,
                    sidebar_state_->width);
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
      EnsureHiddenWindow(profile);
      continue;
    }
    manager->LoadProfileByPath(
        space.path, /*incognito=*/false,
        base::BindOnce(
            [](base::WeakPtr<SpaceController> self, Profile* profile) {
              if (self && profile) {
                self->EnsureHiddenWindow(profile);
              }
            },
            weak_factory_.GetWeakPtr()));
  }
}

BrowserWindowInterface* SpaceController::EnsureHiddenWindow(Profile* profile) {
  if (BrowserWindowInterface* window = GetWindowFor(profile)) {
    return window;
  }
  // Same frame as the visible space window, so the swap shows no jump.
  gfx::Rect bounds;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
        IsVisible(browser)) {
      bounds = FrameOf(browser);
      break;
    }
  }
  SeedSidebarState(profile);
  BrowserWindowCreateParams params(*profile, /*from_user_gesture=*/false);
  params.initial_bounds = bounds;
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return nullptr;
  }
  chrome::AddTabAt(browser, GURL(chrome::kChromeUINewTabURL), -1,
                   /*foreground=*/true, /*group=*/std::nullopt,
                   /*pinned=*/false, NavigateParams::WindowAction::kNoAction);
  return browser;
}

void SpaceController::RequestSwitchTo(BrowserWindowInterface* from,
                                      const std::string& id) {
  // Into or out of native fullscreen the switch is macOS's Space animation;
  // the sidebar's would run before it for nothing.
  Profile* target = GetLoadedProfile(id);
  const bool fullscreen =
      IsFullscreen(from) || IsFullscreen(target ? GetWindowFor(target) : nullptr);
  auto it = sidebars_.find(from);
  if (it != sidebars_.end() && !fullscreen) {
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

void SpaceController::PrepareSwitch(BrowserWindowInterface* from,
                                    const std::string& id) {
  preparing_.clear();
  if (id.empty() || IdOf(from->GetProfile()) == id || IsFullscreen(from)) {
    ClearPrepared(/*hide=*/true);
    return;
  }
  if (Profile* target = GetLoadedProfile(id)) {
    PrepareLoaded(from->GetSessionID(), target);
    return;
  }
  preparing_ = id;
  g_browser_process->profile_manager()->LoadProfileByPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(id),
      /*incognito=*/false,
      base::BindOnce(
          [](base::WeakPtr<SpaceController> self, SessionID from_id,
             std::string id, Profile* profile) {
            if (self && profile && self->preparing_ == id) {
              self->preparing_.clear();
              self->PrepareLoaded(from_id, profile);
            }
          },
          weak_factory_.GetWeakPtr(), from->GetSessionID(), id));
}

void SpaceController::PrepareLoaded(SessionID from_id, Profile* target) {
  BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id);
  if (!from) {
    return;
  }
  BrowserWindowInterface* to = EnsureWindow(target, FrameOf(from));
  if (!to || IsPrepared(to)) {
    return;
  }
  ClearPrepared(/*hide=*/true);
  // Already on screen (another display, fullscreen): nothing to warm.
  if (IsVisible(to) || IsFullscreen(to)) {
    return;
  }
  to->GetWindow()->SetBounds(FrameOf(from));
  SetWindowFaint(to, true);
  to->GetWindow()->ShowInactive();
  prepared_ = to->GetSessionID();
  // Should the sidebar never come back (a renderer gone), it goes.
  prepared_timeout_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(&SpaceController::ClearPrepared, base::Unretained(this),
                     /*hide=*/true));
}

void SpaceController::ClearPrepared(bool hide) {
  prepared_timeout_.Stop();
  if (!prepared_) {
    return;
  }
  BrowserWindowInterface* window =
      BrowserWindowInterface::FromSessionID(*prepared_);
  prepared_.reset();
  if (!window) {
    return;
  }
  if (hide) {
    window->GetWindow()->Hide();
  }
  SetWindowFaint(window, false);
}

bool SpaceController::IsPrepared(BrowserWindowInterface* browser) const {
  return prepared_ && *prepared_ == browser->GetSessionID();
}

void SpaceController::SwitchToLoaded(SessionID from_id, Profile* target) {
  BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id);
  if (!from) {
    return;
  }
  const gfx::Rect bounds = FrameOf(from);
  BrowserWindowInterface* to = EnsureWindow(target, bounds);
  if (!to) {
    return;
  }
  // The prepared window has painted under its 1%: it only turns opaque.
  if (IsPrepared(to)) {
    ClearPrepared(/*hide=*/false);
  } else {
    ClearPrepared(/*hide=*/true);
  }
  target->GetPrefs()->SetTime(kSpaceLastActivePref, base::Time::Now());
  // Native fullscreen (design, "Fullscreen и дисплеи"): every fullscreen
  // space window keeps a macOS Space of its own, and nothing is hidden in
  // one, so each transition runs on the window in front.
  if (IsFullscreen(to)) {
    to->GetWindow()->Activate();
    if (!IsFullscreen(from)) {
      from->GetWindow()->Hide();
    }
    return;
  }
  to->GetWindow()->SetBounds(bounds);
  to->GetWindow()->Show();
  to->GetWindow()->Activate();
  if (IsFullscreen(from)) {
    // Out of the fullscreen Space to the desktop, then into one of its own.
    chrome::ToggleFullscreenMode(to);
    return;
  }
  from->GetWindow()->Hide();
}

BrowserWindowInterface* SpaceController::EnsureWindow(Profile* profile,
                                                      const gfx::Rect& bounds) {
  if (BrowserWindowInterface* window = GetWindowFor(profile)) {
    return window;
  }
  SeedSidebarState(profile);
  BrowserWindowCreateParams params(*profile, /*from_user_gesture=*/true);
  params.initial_bounds = bounds;
  BrowserWindowInterface* window = CreateBrowserWindow(std::move(params));
  if (window) {
    window->OpenGURL(GURL(chrome::kChromeUINewTabURL),
                     WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
  return window;
}

void SpaceController::ShowSpace(
    const std::string& id,
    base::OnceCallback<void(BrowserWindowInterface*)> then) {
  auto show = base::BindOnce(&SpaceController::ShowLoadedSpace,
                             weak_factory_.GetWeakPtr(), std::move(then));
  if (Profile* target = GetLoadedProfile(id)) {
    std::move(show).Run(target);
    return;
  }
  g_browser_process->profile_manager()->LoadProfileByPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(id),
      /*incognito=*/false, std::move(show));
}

void SpaceController::ShowLoadedSpace(
    base::OnceCallback<void(BrowserWindowInterface*)> then,
    Profile* target) {
  if (!target) {
    std::move(then).Run(nullptr);
    return;
  }
  BrowserWindowInterface* visible = GetVisibleSpaceWindow();
  if (visible && visible->GetProfile() != target) {
    SwitchToLoaded(visible->GetSessionID(), target);
  } else if (BrowserWindowInterface* window =
                 visible ? visible : EnsureWindow(target, gfx::Rect())) {
    window->GetWindow()->Show();
    window->GetWindow()->Activate();
    target->GetPrefs()->SetTime(kSpaceLastActivePref, base::Time::Now());
  }
  std::move(then).Run(GetWindowFor(target));
}

BrowserWindowInterface* SpaceController::GetVisibleSpaceWindow() {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (sidebars_.contains(browser) && browser->GetWindow()->IsVisible() &&
            !IsPrepared(browser)) {
          found = browser;
          return false;
        }
        return true;
      });
  return found;
}

void SpaceController::LoadSpace(const std::string& id,
                                base::OnceCallback<void(Profile*)> callback) {
  if (!std::ranges::contains(GetSpaces(), id, &Space::id)) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (Profile* profile = GetLoadedProfile(id)) {
    std::move(callback).Run(profile);
    return;
  }
  g_browser_process->profile_manager()->LoadProfileByPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(id),
      /*incognito=*/false, std::move(callback));
}

void SpaceController::CreateSpace(
    BrowserWindowInterface* from,
    const std::u16string& name,
    const std::string& icon,
    base::OnceCallback<void(Profile*)> configure,
    bool switch_to_it) {
  ProfileManager::CreateMultiProfileAsync(
      name, profiles::GetPlaceholderAvatarIndex(), /*is_hidden=*/false,
      base::BindOnce(&SpaceController::OnSpaceCreated,
                     weak_factory_.GetWeakPtr(), from->GetSessionID(), icon,
                     std::move(configure), switch_to_it));
}

void SpaceController::OnSpaceCreated(
    SessionID from_id,
    std::string icon,
    base::OnceCallback<void(Profile*)> configure,
    bool switch_to_it,
    Profile* profile) {
  if (!profile) {
    return;
  }
  if (configure) {
    std::move(configure).Run(profile);
  }
  profile->GetPrefs()->SetString(kSpaceIconPref, icon);
  profile->GetPrefs()->SetInteger(kSpaceOrderPref,
                                  static_cast<int>(GetSpaces().size()));
  ThemeServiceFactory::GetForProfile(profile)
      ->SetUserColorAndBrowserColorVariant(
          kNewSpaceColors[(GetSpaces().size() - 1) % kNewSpaceColors.size()],
          ui::mojom::BrowserColorVariant::kTonalSpot);
  NotifySpacesChanged();
  if (switch_to_it) {
    SwitchToLoaded(from_id, profile);
  }
}

void SpaceController::RenameSpace(const std::string& id,
                                  const std::u16string& name) {
  ProfileAttributesEntry* entry = Storage().GetProfileAttributesWithPath(
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(id));
  if (entry && !name.empty()) {
    // Notifies through OnProfileNameChanged.
    entry->SetLocalProfileName(name, /*is_default_name=*/false);
  }
}

void SpaceController::SetSpaceIcon(const std::string& id,
                                   const std::string& icon) {
  LoadSpace(id, base::BindOnce(
                      [](base::WeakPtr<SpaceController> self, std::string icon,
                         Profile* profile) {
                        if (self && profile) {
                          profile->GetPrefs()->SetString(kSpaceIconPref, icon);
                          self->NotifySpacesChanged();
                        }
                      },
                      weak_factory_.GetWeakPtr(), icon));
}

void SpaceController::SetSpaceTheme(const std::string& id,
                                    const std::string& theme) {
  LoadSpace(id, base::BindOnce(
                    [](base::WeakPtr<SpaceController> self, std::string theme,
                       Profile* profile) {
                      if (!self || !profile) {
                        return;
                      }
                      profile->GetPrefs()->SetString(kSpaceThemePref, theme);
                      ApplyTheme(profile, theme);
                      self->NotifySpacesChanged();
                    },
                    weak_factory_.GetWeakPtr(), theme));
}

void SpaceController::ReorderSpaces(const std::vector<std::string>& ids) {
  for (size_t i = 0; i < ids.size(); ++i) {
    LoadSpace(ids[i],
                base::BindOnce(
                    [](base::WeakPtr<SpaceController> self, int order,
                       bool last, Profile* profile) {
                      if (!self || !profile) {
                        return;
                      }
                      profile->GetPrefs()->SetInteger(kSpaceOrderPref, order);
                      if (last) {
                        self->NotifySpacesChanged();
                      }
                    },
                    weak_factory_.GetWeakPtr(), static_cast<int>(i),
                    i + 1 == ids.size()));
  }
}

bool SpaceController::DeleteSpace(BrowserWindowInterface* from,
                                  const std::string& id) {
  const std::vector<Space> spaces = GetSpaces();
  auto it = std::ranges::find(spaces, id, &Space::id);
  if (spaces.size() < 2 || it == spaces.end()) {
    return false;
  }
  const base::FilePath path = it->path;
  auto schedule = base::BindOnce(
      [](base::FilePath path, BrowserWindowInterface*) {
        g_browser_process->profile_manager()
            ->GetDeleteProfileHelper()
            .MaybeScheduleProfileForDeletion(
                path, base::DoNothing(),
                ProfileMetrics::DELETE_PROFILE_SETTINGS);
      },
      path);
  if (IdOf(from->GetProfile()) != id) {
    std::move(schedule).Run(nullptr);
    return true;
  }
  // Leave first, so the deleted space's window is a hidden one when it
  // closes and takes no other window with it.
  const Space& neighbour = it + 1 != spaces.end() ? *(it + 1) : *(it - 1);
  ShowSpace(neighbour.id, std::move(schedule));
  return true;
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
        !browser->GetProfile()->IsOffTheRecord() && IsVisible(browser) &&
        !IsPrepared(browser)) {
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

void SpaceController::OnProfileAdded(Profile* profile) {
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
