// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/profiles_adapter.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"

namespace cromarc {

namespace {

ProfileManager* Manager() {
  return g_browser_process->profile_manager();
}

}  // namespace

ProfilesAdapter::ProfilesAdapter() = default;
ProfilesAdapter::~ProfilesAdapter() = default;

base::FilePath ProfilesAdapter::PathOf(const std::string& id) {
  return Manager()->user_data_dir().AppendASCII(id);
}

std::vector<Profiles::Entry> ProfilesAdapter::List() {
  std::vector<Entry> entries;
  for (ProfileAttributesEntry* attributes :
       Manager()->GetProfileAttributesStorage().GetAllProfilesAttributes()) {
    if (attributes->IsOmitted() || attributes->IsEphemeral()) {
      continue;
    }
    Entry entry;
    entry.path = attributes->GetPath();
    entry.id = entry.path.BaseName().AsUTF8Unsafe();
    entry.name = attributes->GetLocalProfileName();
    entry.color =
        attributes->GetProfileThemeColors().profile_highlight_color;
    entries.push_back(std::move(entry));
  }
  return entries;
}

Profile* ProfilesAdapter::GetLoaded(const std::string& id) {
  return Manager()->GetProfileByPath(PathOf(id));
}

void ProfilesAdapter::Load(const std::string& id,
                           base::OnceCallback<void(Profile*)> then) {
  Manager()->LoadProfileByPath(PathOf(id), /*incognito=*/false,
                               std::move(then));
}

std::vector<Profile*> ProfilesAdapter::Loaded() {
  return Manager()->GetLoadedProfiles();
}

Profile* ProfilesAdapter::FromBrowserContext(content::BrowserContext* context) {
  return Profile::FromBrowserContext(context);
}

base::FilePath ProfilesAdapter::UserDataDir() {
  return Manager()->user_data_dir();
}

std::string ProfilesAdapter::IdOf(Profile* profile) {
  return profile->GetOriginalProfile()->GetBaseName().AsUTF8Unsafe();
}

bool ProfilesAdapter::IsOffTheRecord(Profile* profile) {
  return profile->IsOffTheRecord();
}

PrefService* ProfilesAdapter::PrefsOf(Profile* profile) {
  return profile->GetPrefs();
}

void ProfilesAdapter::Create(const std::u16string& name,
                             base::OnceCallback<void(Profile*)> then) {
  ProfileManager::CreateMultiProfileAsync(
      name, profiles::GetPlaceholderAvatarIndex(), /*is_hidden=*/false,
      std::move(then));
}

void ProfilesAdapter::Rename(const std::string& id,
                             const std::u16string& name) {
  if (ProfileAttributesEntry* entry =
          Manager()->GetProfileAttributesStorage().GetProfileAttributesWithPath(
              PathOf(id))) {
    // Notifies through OnProfileNameChanged.
    entry->SetLocalProfileName(name, /*is_default_name=*/false);
  }
}

void ProfilesAdapter::Delete(const std::string& id) {
  Manager()->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      PathOf(id), base::DoNothing(), ProfileMetrics::DELETE_PROFILE_SETTINGS);
}

base::CallbackListSubscription ProfilesAdapter::OnChanged(
    base::RepeatingClosure callback) {
  if (!profile_manager_observation_.IsObserving()) {
    storage_observation_.Observe(&Manager()->GetProfileAttributesStorage());
    profile_manager_observation_.Observe(Manager());
  }
  return on_changed_.Add(std::move(callback));
}

void ProfilesAdapter::OnProfileAdded(const base::FilePath& profile_path) {
  on_changed_.Notify();
}

void ProfilesAdapter::OnProfileWasRemoved(const base::FilePath& profile_path,
                                          const std::u16string& profile_name) {
  on_changed_.Notify();
}

void ProfilesAdapter::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  on_changed_.Notify();
}

void ProfilesAdapter::OnProfileAdded(Profile* profile) {
  on_changed_.Notify();
}

void ProfilesAdapter::OnProfileManagerDestroying() {
  storage_observation_.Reset();
  profile_manager_observation_.Reset();
}

}  // namespace cromarc
