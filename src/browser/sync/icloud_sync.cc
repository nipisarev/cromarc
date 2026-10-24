// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/sync/icloud_sync.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "cromarc/browser/spaces/space_prefs.h"

namespace cromarc {

namespace {

constexpr char kRootSwitch[] = "cromarc-icloud-dir";
constexpr char kSpaceFile[] = "space.json";
// Chromium's own name for it (kLoopbackServerBackendFilename).
constexpr char kBackendFile[] = "profile.pb";

// Everything but what a password manager or the Mac keeps.
constexpr syncer::UserSelectableTypeSet kTypes = {
    syncer::UserSelectableType::kBookmarks,
    syncer::UserSelectableType::kPreferences,
    syncer::UserSelectableType::kAutofill,
    syncer::UserSelectableType::kThemes,
    syncer::UserSelectableType::kHistory,
    syncer::UserSelectableType::kExtensions,
    syncer::UserSelectableType::kApps,
    syncer::UserSelectableType::kReadingList,
    syncer::UserSelectableType::kTabs,
    syncer::UserSelectableType::kSavedTabGroups,
};

void WriteSpaceFile(const base::FilePath& dir, std::string json) {
  base::CreateDirectory(dir);
  base::ImportantFileWriter::WriteFileAtomically(dir.AppendASCII(kSpaceFile),
                                                 json);
}

std::vector<ICloudSync::RemoteSpace> ReadSpaces(const base::FilePath& root) {
  std::vector<ICloudSync::RemoteSpace> spaces;
  base::FileEnumerator dirs(root, /*recursive=*/false,
                            base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir = dirs.Next(); !dir.empty(); dir = dirs.Next()) {
    std::string json;
    if (!base::ReadFileToString(dir.AppendASCII(kSpaceFile), &json)) {
      continue;
    }
    std::optional<base::Value> value = base::JSONReader::Read(json, 0);
    const base::DictValue* dict = value ? value->GetIfDict() : nullptr;
    const std::string* uuid = dict ? dict->FindString("uuid") : nullptr;
    if (!uuid || dir.BaseName().AsUTF8Unsafe() != *uuid) {
      continue;
    }
    const std::string* name = dict->FindString("name");
    const std::string* icon = dict->FindString("icon");
    spaces.push_back({*uuid, name ? *name : *uuid, icon ? *icon : ""});
  }
  return spaces;
}

}  // namespace

// static
ICloudSync* ICloudSync::Get(Profile* profile) {
  return ICloudSyncFactory::GetForProfile(profile);
}

// static
base::FilePath ICloudSync::Root() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kRootSwitch)) {
    return command_line->GetSwitchValuePath(kRootSwitch);
  }
  return base::GetHomeDir().Append(
      "Library/Mobile Documents/com~apple~CloudDocs/Cromarc");
}

// static
void ICloudSync::FindSpaces(
    base::OnceCallback<void(std::vector<RemoteSpace>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadSpaces, Root()), std::move(callback));
}

ICloudSync::ICloudSync(Profile* profile, syncer::SyncService* sync_service)
    : profile_(profile), sync_service_(sync_service) {
  ConfigureSync();
}

ICloudSync::~ICloudSync() = default;

bool ICloudSync::enabled() const {
  return profile_->GetPrefs()->GetBoolean(
      syncer::prefs::kEnableLocalSyncBackend);
}

bool ICloudSync::active() const {
  return sync_service_ && sync_service_->IsLocalSyncEnabled();
}

void ICloudSync::SetEnabled(bool enabled) {
  PrefService* prefs = profile_->GetPrefs();
  if (!enabled) {
    prefs->SetBoolean(syncer::prefs::kEnableLocalSyncBackend, false);
    return;
  }
  const base::FilePath dir = Root().AppendASCII(Uuid());
  // Despite the name, on the Mac the pref is the backend's file.
  prefs->SetFilePath(syncer::prefs::kLocalSyncBackendDir,
                     dir.AppendASCII(kBackendFile));
  prefs->SetBoolean(syncer::prefs::kEnableLocalSyncBackend, true);
  std::string json;
  base::JSONWriter::Write(
      base::DictValue()
          .Set("uuid", Uuid())
          .Set("name", Name())
          .Set("icon", profile_->GetPrefs()->GetString(kSpaceIconPref)),
      &json);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WriteSpaceFile, dir, std::move(json)));
}

std::string ICloudSync::Name() const {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  return entry ? base::UTF16ToUTF8(entry->GetLocalProfileName())
               : std::string();
}

std::string ICloudSync::Uuid() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->GetString(kSpaceUuidPref).empty()) {
    prefs->SetString(kSpaceUuidPref,
                     base::Uuid::GenerateRandomV4().AsLowercaseString());
  }
  return prefs->GetString(kSpaceUuidPref);
}

void ICloudSync::SetUuid(const std::string& uuid) {
  profile_->GetPrefs()->SetString(kSpaceUuidPref, uuid);
}

void ICloudSync::ConfigureSync() {
  if (!active()) {
    return;
  }
  syncer::SyncUserSettings* settings = sync_service_->GetUserSettings();
  if (settings->IsInitialSyncFeatureSetupComplete()) {
    return;
  }
  settings->SetSelectedTypes(/*sync_everything=*/false, kTypes);
  settings->SetInitialSyncFeatureSetupComplete();
}

// static
ICloudSyncFactory* ICloudSyncFactory::GetInstance() {
  static base::NoDestructor<ICloudSyncFactory> instance;
  return instance.get();
}

// static
ICloudSync* ICloudSyncFactory::GetForProfile(Profile* profile) {
  return static_cast<ICloudSync*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ICloudSyncFactory::ICloudSyncFactory()
    : ProfileKeyedServiceFactory("CromarcICloudSync",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

ICloudSyncFactory::~ICloudSyncFactory() = default;

void ICloudSyncFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kSpaceUuidPref, std::string());
}

std::unique_ptr<KeyedService>
ICloudSyncFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ICloudSync>(
      profile, SyncServiceFactory::GetForProfile(profile));
}

bool ICloudSyncFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace cromarc
