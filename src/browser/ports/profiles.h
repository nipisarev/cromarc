// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_PROFILES_H_
#define CROMARC_BROWSER_PORTS_PROFILES_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace cromarc {

// The browser's profiles, which are Cromarc's spaces (design, premise 1).
// Profile pointers are handles only. A profile's id is its directory's base
// name ("Default", "Profile 1").
class Profiles {
 public:
  struct Entry {
    std::string id;
    base::FilePath path;
    std::u16string name;
    SkColor color = SK_ColorTRANSPARENT;
  };

  virtual ~Profiles() = default;

  // Every profile but the omitted and ephemeral ones, loaded or not.
  virtual std::vector<Entry> List() = 0;
  // Null when that profile is not loaded.
  virtual Profile* GetLoaded(const std::string& id) = 0;
  // Loads the profile; null for none.
  virtual void Load(const std::string& id,
                    base::OnceCallback<void(Profile*)> then) = 0;
  virtual std::vector<Profile*> Loaded() = 0;

  // The profile `context` is.
  virtual Profile* FromBrowserContext(content::BrowserContext* context) = 0;
  // Where the profiles' directories are, and Cromarc's own files.
  virtual base::FilePath UserDataDir() = 0;
  // The id of `profile`, or of the profile it is off the record of.
  virtual std::string IdOf(Profile* profile) = 0;
  virtual bool IsOffTheRecord(Profile* profile) = 0;
  virtual PrefService* PrefsOf(Profile* profile) = 0;

  // A new profile named `name` with the placeholder avatar; null on failure.
  virtual void Create(const std::u16string& name,
                      base::OnceCallback<void(Profile*)> then) = 0;
  virtual void Rename(const std::string& id, const std::u16string& name) = 0;
  // Deletes the profile once its windows are closed.
  virtual void Delete(const std::string& id) = 0;

  // Called when a profile is added, loaded, removed or renamed.
  virtual base::CallbackListSubscription OnChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_PROFILES_H_
