// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/spaces/space_controller.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/ports/strip_defaults.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/theme.h"
#include "cromarc/browser/spaces/space_pref_names.h"
#include "cromarc/browser/spaces/space_window.h"

namespace cromarc {

namespace {

// Browser suites (tooling/test-browser) read what a switch did from the log:
// in headless mode a hidden window's pages may still report "visible".
constexpr char kLogSpacesSwitch[] = "cromarc-test-log-spaces";

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

// A space theme's "scheme": "light", "dark", or anything else for the system's.
Theme::Scheme SchemeOf(const std::string& scheme) {
  return scheme == "light"  ? Theme::Scheme::kLight
         : scheme == "dark" ? Theme::Scheme::kDark
                            : Theme::Scheme::kSystem;
}

}  // namespace

SpaceController::SpaceController(Profiles& profiles,
                                 Windows& windows,
                                 Tabs& tabs,
                                 Theme& theme,
                                 StripDefaults& strip_defaults)
    : profiles_(profiles),
      windows_(windows),
      tabs_(tabs),
      theme_(theme),
      strip_defaults_(strip_defaults),
      sidebar_state_(base::BindRepeating(
          [](SpaceController* self) {
            for (Profile* profile : self->profiles_->Loaded()) {
              self->SeedSidebarState(profile);
            }
          },
          base::Unretained(this))) {
  // A profile added, removed or renamed; or loaded, when its space's icon,
  // theme and place in the order become known (they are in its prefs).
  profiles_changed_ = profiles_->OnChanged(base::BindRepeating(
      &SpaceController::NotifySpacesChanged, base::Unretained(this)));
}

SpaceController::~SpaceController() = default;

bool SpaceController::IsOnScreen(BrowserWindowInterface* browser) {
  return windows_->IsOnScreen(browser);
}

bool SpaceController::IsFullscreen(BrowserWindowInterface* browser) {
  return browser && windows_->IsFullscreen(browser);
}

gfx::Rect SpaceController::FrameOf(BrowserWindowInterface* browser) {
  return IsFullscreen(browser) ? windows_->RestoredBoundsOf(browser)
                               : windows_->BoundsOf(browser);
}

void SpaceController::ApplyTheme(Profile* profile, const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json, 0);
  const base::DictValue* theme = value ? value->GetIfDict() : nullptr;
  if (!theme) {
    return;
  }
  const base::ListValue* colors = theme->FindList("colors");
  if (colors && !colors->empty() && (*colors)[0].is_list() &&
      (*colors)[0].GetList().size() == 3) {
    const base::ListValue& rgb = (*colors)[0].GetList();
    theme_->SetColor(profile, SkColorSetRGB(rgb[0].GetIfInt().value_or(0),
                                            rgb[1].GetIfInt().value_or(0),
                                            rgb[2].GetIfInt().value_or(0)));
  }
  const std::string* scheme = theme->FindString("scheme");
  theme_->SetScheme(profile, SchemeOf(scheme ? *scheme : ""));
}



std::vector<SpaceController::Space> SpaceController::GetSpaces() {
  struct Ordered {
    Space space;
    int order;
  };
  std::vector<Ordered> spaces;
  for (Profiles::Entry& entry : profiles_->List()) {
    Space space;
    space.path = std::move(entry.path);
    space.id = std::move(entry.id);
    space.name = std::move(entry.name);
    space.color = entry.color;
    int order = -1;
    if (Profile* profile = profiles_->GetLoaded(space.id)) {
      PrefService* prefs = profiles_->PrefsOf(profile);
      space.icon = prefs->GetString(kSpaceIconPref);
      space.theme = prefs->GetString(kSpaceThemePref);
      order = prefs->GetInteger(kSpaceOrderPref);
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
  return profiles_->GetLoaded(space_id);
}

BrowserWindowInterface* SpaceController::WindowFor(Profile* profile) {
  for (BrowserWindowInterface* browser : windows_->AllByActivation()) {
    if (windows_->ProfileOf(browser) == profile &&
        windows_->IsNormal(browser)) {
      return browser;
    }
  }
  return nullptr;
}

void SpaceController::AddSidebar(BrowserWindowInterface* browser,
                                 Sidebar* sidebar) {
  sidebars_[browser] = sidebar;
  close_subscriptions_[browser] = windows_->OnClosed(
      browser, base::BindRepeating(&SpaceController::OnSpaceWindowClosed,
                                   base::Unretained(this)));
}

void SpaceController::RemoveSidebar(BrowserWindowInterface* browser) {
  sidebars_.erase(browser);
}

base::ScopedClosureRunner SpaceController::ShareSidebarState(
    VerticalStrip& strip) {
  return sidebar_state_.Add(strip);
}

void SpaceController::SeedSidebarState(Profile* profile) {
  const std::optional<SharedSidebarState::State>& state =
      sidebar_state_.state();
  if (!state || profiles_->IsOffTheRecord(profile)) {
    return;
  }
  strip_defaults_->Set(profile, state->collapsed, state->width);
}

void SpaceController::OnSpaceWindowClosed(BrowserWindowInterface* browser) {
  close_subscriptions_.erase(browser);
  if (!IsOnScreen(browser)) {
    return;
  }
  for (BrowserWindowInterface* other : windows_->All()) {
    if (other != browser && windows_->IsNormal(other) &&
        !windows_->IsOffTheRecord(other) && !IsOnScreen(other)) {
      windows_->Close(other);
    }
  }
}

bool SpaceController::KeepWindowOnLastTabClose(
    BrowserWindowInterface* browser) {
  if (!windows_->IsNormal(browser) || windows_->IsOffTheRecord(browser)) {
    return false;
  }
  const std::vector<TabId> tabs = tabs_->List(browser);
  if (tabs.size() != 1) {
    return false;
  }
  tabs_->AddNewTabPage(browser, /*keep_window_hidden=*/false);
  tabs_->CloseByUser(tabs[0]);
  return true;
}

void SpaceController::PrepareWarmWindows() {
  for (const Space& space : GetSpaces()) {
    if (Profile* profile = profiles_->GetLoaded(space.id)) {
      EnsureHiddenWindow(profile);
      continue;
    }
    profiles_->Load(
        space.id,
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
  if (BrowserWindowInterface* window = WindowFor(profile)) {
    return window;
  }
  // Same frame as the visible space window, so the swap shows no jump.
  gfx::Rect bounds;
  for (BrowserWindowInterface* browser : windows_->All()) {
    if (windows_->IsNormal(browser) && IsOnScreen(browser)) {
      bounds = FrameOf(browser);
      break;
    }
  }
  SeedSidebarState(profile);
  BrowserWindowInterface* browser =
      windows_->Create(profile, bounds, /*from_user_gesture=*/false);
  if (!browser) {
    return nullptr;
  }
  tabs_->AddNewTabPage(browser, /*keep_window_hidden=*/true);
  return browser;
}

void SpaceController::RequestSwitchTo(BrowserWindowInterface* from,
                                      const std::string& id) {
  // Into or out of native fullscreen the switch is macOS's Space animation;
  // the sidebar's would run before it for nothing.
  Profile* target = GetLoadedProfile(id);
  const bool fullscreen =
      IsFullscreen(from) || IsFullscreen(target ? WindowFor(target) : nullptr);
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
  const std::string current = profiles_->IdOf(windows_->ProfileOf(from));
  auto it = std::ranges::find(spaces, current, &Space::id);
  const int count = static_cast<int>(spaces.size());
  const int index = it == spaces.end() ? 0 : it - spaces.begin();
  RequestSwitchTo(from, spaces[(index + step % count + count) % count].id);
}

void SpaceController::SwitchTo(BrowserWindowInterface* from,
                               const std::string& id) {
  if (profiles_->IdOf(windows_->ProfileOf(from)) == id) {
    return;
  }
  if (Profile* target = GetLoadedProfile(id)) {
    SwitchToLoaded(windows_->IdOf(from), target);
    return;
  }
  // Cold: the space's profile loads first.
  const WindowId from_id = windows_->IdOf(from);
  profiles_->Load(
      id,
      base::BindOnce(
          [](base::WeakPtr<SpaceController> self, WindowId from_id,
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
  if (id.empty() || profiles_->IdOf(windows_->ProfileOf(from)) == id ||
      IsFullscreen(from)) {
    ClearPrepared(/*hide=*/true);
    return;
  }
  if (Profile* target = GetLoadedProfile(id)) {
    PrepareLoaded(windows_->IdOf(from), target);
    return;
  }
  preparing_ = id;
  profiles_->Load(
      id,
      base::BindOnce(
          [](base::WeakPtr<SpaceController> self, WindowId from_id,
             std::string id, Profile* profile) {
            if (self && profile && self->preparing_ == id) {
              self->preparing_.clear();
              self->PrepareLoaded(from_id, profile);
            }
          },
          weak_factory_.GetWeakPtr(), windows_->IdOf(from), id));
}

void SpaceController::PrepareLoaded(WindowId from_id, Profile* target) {
  BrowserWindowInterface* from = windows_->FromId(from_id);
  if (!from) {
    return;
  }
  BrowserWindowInterface* to = EnsureWindow(target, FrameOf(from));
  if (!to || IsPrepared(to)) {
    return;
  }
  ClearPrepared(/*hide=*/true);
  // Already on screen (another display, fullscreen): nothing to warm.
  if (IsOnScreen(to) || IsFullscreen(to)) {
    return;
  }
  windows_->SetBounds(to, FrameOf(from));
  SetWindowFaint(windows_->NativeWindowOf(to), true);
  windows_->ShowInactive(to);
  prepared_ = windows_->IdOf(to);
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
  BrowserWindowInterface* window = windows_->FromId(*prepared_);
  prepared_.reset();
  if (!window) {
    return;
  }
  if (hide) {
    windows_->Hide(window);
  }
  SetWindowFaint(windows_->NativeWindowOf(window), false);
}

bool SpaceController::IsPrepared(BrowserWindowInterface* browser) const {
  return prepared_ && *prepared_ == windows_->IdOf(browser);
}

void SpaceController::SwitchToLoaded(WindowId from_id, Profile* target) {
  BrowserWindowInterface* from = windows_->FromId(from_id);
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
  profiles_->PrefsOf(target)->SetTime(kSpaceLastActivePref, base::Time::Now());
  // Native fullscreen (design, "Fullscreen и дисплеи"): every fullscreen
  // space window keeps a macOS Space of its own, and nothing is hidden in
  // one, so each transition runs on the window in front.
  if (IsFullscreen(to)) {
    windows_->Activate(to);
    if (!IsFullscreen(from)) {
      windows_->Hide(from);
    }
    return;
  }
  windows_->SetBounds(to, bounds);
  windows_->Show(to);
  windows_->Activate(to);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kLogSpacesSwitch)) {
    LOG(ERROR) << "cromarc space switch: " << profiles_->IdOf(windows_->ProfileOf(from))
               << " -> " << profiles_->IdOf(target);
  }
  if (IsFullscreen(from)) {
    // Out of the fullscreen Space to the desktop, then into one of its own.
    windows_->ToggleFullscreen(to);
    return;
  }
  windows_->Hide(from);
}

BrowserWindowInterface* SpaceController::EnsureWindow(Profile* profile,
                                                      const gfx::Rect& bounds) {
  if (BrowserWindowInterface* window = WindowFor(profile)) {
    return window;
  }
  SeedSidebarState(profile);
  BrowserWindowInterface* window =
      windows_->Create(profile, bounds, /*from_user_gesture=*/true);
  if (window) {
    tabs_->OpenNewTabPage(window);
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
  profiles_->Load(id, std::move(show));
}

void SpaceController::ShowLoadedSpace(
    base::OnceCallback<void(BrowserWindowInterface*)> then,
    Profile* target) {
  if (!target) {
    std::move(then).Run(nullptr);
    return;
  }
  BrowserWindowInterface* visible = GetVisibleSpaceWindow();
  if (visible && windows_->ProfileOf(visible) != target) {
    SwitchToLoaded(windows_->IdOf(visible), target);
  } else if (BrowserWindowInterface* window =
                 visible ? visible : EnsureWindow(target, gfx::Rect())) {
    windows_->Show(window);
    windows_->Activate(window);
    profiles_->PrefsOf(target)->SetTime(kSpaceLastActivePref,
                                        base::Time::Now());
  }
  std::move(then).Run(WindowFor(target));
}

BrowserWindowInterface* SpaceController::GetVisibleSpaceWindow() {
  for (BrowserWindowInterface* browser : windows_->AllByActivation()) {
    if (sidebars_.contains(browser) && windows_->IsVisible(browser) &&
        !IsPrepared(browser)) {
      return browser;
    }
  }
  return nullptr;
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
  profiles_->Load(id, std::move(callback));
}

void SpaceController::CreateSpace(
    BrowserWindowInterface* from,
    const std::u16string& name,
    const std::string& icon,
    base::OnceCallback<void(Profile*)> configure,
    bool switch_to_it) {
  profiles_->Create(
      name, base::BindOnce(&SpaceController::OnSpaceCreated,
                           weak_factory_.GetWeakPtr(), windows_->IdOf(from),
                           icon, std::move(configure), switch_to_it));
}

void SpaceController::OnSpaceCreated(
    WindowId from_id,
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
  PrefService* prefs = profiles_->PrefsOf(profile);
  prefs->SetString(kSpaceIconPref, icon);
  prefs->SetInteger(kSpaceOrderPref, static_cast<int>(GetSpaces().size()));
  theme_->SetColor(
      profile,
      kNewSpaceColors[(GetSpaces().size() - 1) % kNewSpaceColors.size()]);
  NotifySpacesChanged();
  if (switch_to_it) {
    SwitchToLoaded(from_id, profile);
  }
}

void SpaceController::RenameSpace(const std::string& id,
                                  const std::u16string& name) {
  if (!name.empty()) {
    // Notifies through Profiles::OnChanged.
    profiles_->Rename(id, name);
  }
}

void SpaceController::SetSpaceIcon(const std::string& id,
                                   const std::string& icon) {
  LoadSpace(id, base::BindOnce(
                      [](base::WeakPtr<SpaceController> self, std::string icon,
                         Profile* profile) {
                        if (self && profile) {
                          self->profiles_->PrefsOf(profile)->SetString(
                              kSpaceIconPref, icon);
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
                      self->profiles_->PrefsOf(profile)->SetString(
                          kSpaceThemePref, theme);
                      self->ApplyTheme(profile, theme);
                      self->NotifySpacesChanged();
                    },
                    weak_factory_.GetWeakPtr(), theme));
}

void SpaceController::SetSpaceScheme(const std::string& id,
                                     const std::string& scheme) {
  LoadSpace(id, base::BindOnce(
                    [](base::WeakPtr<SpaceController> self, std::string scheme,
                       Profile* profile) {
                      if (self && profile) {
                        self->theme_->SetScheme(profile, SchemeOf(scheme));
                      }
                    },
                    weak_factory_.GetWeakPtr(), scheme));
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
                      self->profiles_->PrefsOf(profile)->SetInteger(
                          kSpaceOrderPref, order);
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
  // The ports outlive the switch: they are the browser's.
  auto schedule = base::BindOnce(
      [](Profiles* profiles, std::string id, BrowserWindowInterface*) {
        profiles->Delete(id);
      },
      base::Unretained(&profiles_.get()), id);
  if (profiles_->IdOf(windows_->ProfileOf(from)) != id) {
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
  if (!warm_windows_scheduled_ && !windows_->IsOffTheRecord(browser)) {
    warm_windows_scheduled_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpaceController::PrepareWarmWindows,
                       weak_factory_.GetWeakPtr()),
        kWarmWindowsDelay);
  }
  if (base::TimeTicks::Now() - started_ > kStartupWindow) {
    profiles_->PrefsOf(windows_->ProfileOf(browser))
        ->SetTime(kSpaceLastActivePref, base::Time::Now());
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
  for (BrowserWindowInterface* browser : windows_->All()) {
    if (windows_->IsNormal(browser) && !windows_->IsOffTheRecord(browser) &&
        IsOnScreen(browser) && !IsPrepared(browser)) {
      visible.push_back(browser);
    }
  }
  if (visible.size() < 2) {
    return;
  }
  BrowserWindowInterface* keep = *std::ranges::max_element(
      visible, {}, [this](BrowserWindowInterface* browser) {
        return profiles_->PrefsOf(windows_->ProfileOf(browser))
            ->GetTime(kSpaceLastActivePref);
      });
  const gfx::Rect bounds = windows_->BoundsOf(keep);
  for (BrowserWindowInterface* browser : visible) {
    if (windows_->ProfileOf(browser) != windows_->ProfileOf(keep)) {
      windows_->SetBounds(browser, bounds);
      windows_->Hide(browser);
    }
  }
}

void SpaceController::NotifySpacesChanged() {
  for (auto& [browser, sidebar] : sidebars_) {
    sidebar->OnSpacesChanged();
  }
}

}  // namespace cromarc
