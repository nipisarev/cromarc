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
#include "components/prefs/pref_service.h"
#include "cromarc/browser/ports/local_sync.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/spaces/space_pref_names.h"

namespace cromarc {

namespace {

constexpr char kRootSwitch[] = "cromarc-icloud-dir";
constexpr char kSpaceFile[] = "space.json";
// Chromium's own name for it (kLoopbackServerBackendFilename).
constexpr char kBackendFile[] = "profile.pb";

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

ICloudSync::ICloudSync(Profile* profile,
                       Profiles& profiles,
                       std::unique_ptr<LocalSync> local_sync)
    : profile_(profile),
      profiles_(profiles),
      local_sync_(std::move(local_sync)) {
  // The first start with the local backend.
  local_sync_->SetUpOnce();
}

ICloudSync::~ICloudSync() = default;

bool ICloudSync::enabled() const {
  return local_sync_->IsEnabled();
}

bool ICloudSync::active() const {
  return local_sync_->IsActive();
}

void ICloudSync::SetEnabled(bool enabled) {
  if (!enabled) {
    local_sync_->Disable();
    return;
  }
  const base::FilePath dir = Root().AppendASCII(Uuid());
  local_sync_->Enable(dir.AppendASCII(kBackendFile));
  std::string json;
  base::JSONWriter::Write(
      base::DictValue()
          .Set("uuid", Uuid())
          .Set("name", Name())
          .Set("icon", profiles_->PrefsOf(profile_)->GetString(kSpaceIconPref)),
      &json);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WriteSpaceFile, dir, std::move(json)));
}

std::string ICloudSync::Name() const {
  const std::string id = profiles_->IdOf(profile_);
  for (const Profiles::Entry& entry : profiles_->List()) {
    if (entry.id == id) {
      return base::UTF16ToUTF8(entry.name);
    }
  }
  return std::string();
}

std::string ICloudSync::Uuid() {
  PrefService* prefs = profiles_->PrefsOf(profile_);
  if (prefs->GetString(kSpaceUuidPref).empty()) {
    prefs->SetString(kSpaceUuidPref,
                     base::Uuid::GenerateRandomV4().AsLowercaseString());
  }
  return prefs->GetString(kSpaceUuidPref);
}

void ICloudSync::SetUuid(const std::string& uuid) {
  profiles_->PrefsOf(profile_)->SetString(kSpaceUuidPref, uuid);
}

}  // namespace cromarc
