// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_profiles.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace cromarc {

namespace {

Profile* HandleAt(size_t index) {
  return reinterpret_cast<Profile*>(static_cast<uintptr_t>(0x100 * (index + 1)));
}

}  // namespace

FakeProfiles::FakeProfiles() = default;
FakeProfiles::~FakeProfiles() = default;

FakeProfiles::Space* FakeProfiles::Find(const std::string& id) {
  auto it = std::ranges::find(spaces_, id, &Space::id);
  return it == spaces_.end() ? nullptr : &*it;
}

FakeProfiles::Space* FakeProfiles::Find(Profile* profile) {
  for (size_t i = 0; i < spaces_.size(); ++i) {
    if (HandleAt(i) == profile) {
      return &spaces_[i];
    }
  }
  return nullptr;
}

Profile* FakeProfiles::Add(const std::string& id,
                           const std::u16string& name,
                           bool loaded) {
  Space space{id, name, loaded, std::make_unique<TestingPrefServiceSimple>()};
  if (register_prefs) {
    register_prefs.Run(space.prefs->registry());
  }
  spaces_.push_back(std::move(space));
  return HandleAt(spaces_.size() - 1);
}

Profile* FakeProfiles::HandleOf(const std::string& id) {
  for (size_t i = 0; i < spaces_.size(); ++i) {
    if (spaces_[i].id == id) {
      return HandleAt(i);
    }
  }
  return nullptr;
}

TestingPrefServiceSimple& FakeProfiles::prefs(Profile* profile) {
  Space* space = Find(profile);
  CHECK(space);
  return *space->prefs;
}

std::vector<Profiles::Entry> FakeProfiles::List() {
  std::vector<Entry> entries;
  for (const Space& space : spaces_) {
    entries.push_back(
        {space.id, base::FilePath::FromASCII(space.id), space.name});
  }
  return entries;
}

Profile* FakeProfiles::GetLoaded(const std::string& id) {
  Space* space = Find(id);
  return space && space->loaded ? HandleOf(id) : nullptr;
}

void FakeProfiles::Load(const std::string& id,
                        base::OnceCallback<void(Profile*)> then) {
  Space* space = Find(id);
  if (space) {
    space->loaded = true;
  }
  std::move(then).Run(space ? HandleOf(id) : nullptr);
}

std::vector<Profile*> FakeProfiles::Loaded() {
  std::vector<Profile*> loaded;
  for (const Space& space : spaces_) {
    if (space.loaded) {
      loaded.push_back(HandleOf(space.id));
    }
  }
  return loaded;
}

std::string FakeProfiles::IdOf(Profile* profile) {
  Space* space = Find(profile);
  return space ? space->id : std::string();
}

bool FakeProfiles::IsOffTheRecord(Profile* profile) {
  return false;
}

PrefService* FakeProfiles::PrefsOf(Profile* profile) {
  return &prefs(profile);
}

void FakeProfiles::Create(const std::u16string& name,
                          base::OnceCallback<void(Profile*)> then) {
  Profile* profile =
      Add("Profile " + base::NumberToString(spaces_.size()), name);
  on_changed_.Notify();
  std::move(then).Run(profile);
}

void FakeProfiles::Rename(const std::string& id, const std::u16string& name) {
  if (Space* space = Find(id)) {
    space->name = name;
    on_changed_.Notify();
  }
}

void FakeProfiles::Delete(const std::string& id) {
  deleted.push_back(id);
}

base::CallbackListSubscription FakeProfiles::OnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

}  // namespace cromarc
