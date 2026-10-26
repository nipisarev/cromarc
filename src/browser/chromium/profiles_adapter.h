// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_PROFILES_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_PROFILES_ADAPTER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "cromarc/browser/ports/profiles.h"

class ProfileAttributesStorage;
class ProfileManager;

namespace cromarc {

// Profiles on Chrome's ProfileManager and its attributes storage.
class ProfilesAdapter : public Profiles,
                        public ProfileAttributesStorageObserver,
                        public ProfileManagerObserver {
 public:
  ProfilesAdapter();
  ProfilesAdapter(const ProfilesAdapter&) = delete;
  ProfilesAdapter& operator=(const ProfilesAdapter&) = delete;
  ~ProfilesAdapter() override;

  // Profiles:
  std::vector<Entry> List() override;
  Profile* GetLoaded(const std::string& id) override;
  void Load(const std::string& id,
            base::OnceCallback<void(Profile*)> then) override;
  std::vector<Profile*> Loaded() override;
  std::string IdOf(Profile* profile) override;
  bool IsOffTheRecord(Profile* profile) override;
  PrefService* PrefsOf(Profile* profile) override;
  void Create(const std::u16string& name,
              base::OnceCallback<void(Profile*)> then) override;
  void Rename(const std::string& id, const std::u16string& name) override;
  void Delete(const std::string& id) override;
  base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) override;

  // ProfileAttributesStorageObserver:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

  // ProfileManagerObserver: a space's icon, theme and place in the order are
  // in its profile's prefs, known once it has loaded.
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::FilePath PathOf(const std::string& id);

  base::RepeatingClosureList on_changed_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      storage_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_PROFILES_ADAPTER_H_
