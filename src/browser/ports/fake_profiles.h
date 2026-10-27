// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_FAKE_PROFILES_H_
#define CROMARC_BROWSER_PORTS_FAKE_PROFILES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "components/prefs/testing_pref_service.h"
#include "cromarc/browser/ports/profiles.h"

namespace cromarc {

// Profiles whose handles point at nothing, each with real (testing) prefs.
class FakeProfiles : public Profiles {
 public:
  FakeProfiles();
  ~FakeProfiles() override;

  // A profile `id` named `name`, loaded unless `loaded` is false; its handle.
  Profile* Add(const std::string& id,
               const std::u16string& name,
               bool loaded = true);
  // The handle `id` has once loaded, loaded or not.
  Profile* HandleOf(const std::string& id);
  TestingPrefServiceSimple& prefs(Profile* profile);

  // Profiles:
  std::vector<Entry> List() override;
  std::u16string NameOf(const std::string& id) override;
  // A fake profile's name is always one a test gave it.
  bool HasDefaultName(const std::string& id) override;
  Profile* GetLoaded(const std::string& id) override;
  void Load(const std::string& id,
            base::OnceCallback<void(Profile*)> then) override;
  std::vector<Profile*> Loaded() override;
  Profile* FromBrowserContext(content::BrowserContext* context) override;
  base::FilePath UserDataDir() override;
  std::string IdOf(Profile* profile) override;
  bool IsOffTheRecord(Profile* profile) override;
  PrefService* PrefsOf(Profile* profile) override;
  void Create(const std::u16string& name,
              base::OnceCallback<void(Profile*)> then) override;
  void Rename(const std::string& id, const std::u16string& name) override;
  void Delete(const std::string& id) override;
  base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) override;

  // Runs on every new profile's prefs: a test registers its prefs here.
  base::RepeatingCallback<void(PrefRegistrySimple*)> register_prefs;
  std::vector<std::string> deleted;

 private:
  struct Space {
    std::string id;
    std::u16string name;
    bool loaded = true;
    std::unique_ptr<TestingPrefServiceSimple> prefs;
  };

  Space* Find(const std::string& id);
  Space* Find(Profile* profile);

  // In List() order.
  std::vector<Space> spaces_;
  base::RepeatingClosureList on_changed_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_FAKE_PROFILES_H_
