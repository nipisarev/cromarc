// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/arc_import/arc_import_page_handler.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/prefs/pref_service.h"
#include "cromarc/browser/arc_import/arc_import.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/spaces/space_prefs.h"
#include "cromarc/browser/tree/tree_service.h"

namespace cromarc {

namespace {

// Tests point the page at a fixture instead of the user's Arc data.
constexpr char kArcSidebarSwitch[] = "cromarc-arc-sidebar";

base::FilePath ArcSidebarPath() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kArcSidebarSwitch)) {
    return command_line->GetSwitchValuePath(kArcSidebarSwitch);
  }
  return base::GetHomeDir().Append(
      "Library/Application Support/Arc/StorableSidebar.json");
}

std::optional<ArcSidebar> ReadArcSidebar(const base::FilePath& path) {
  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    return std::nullopt;
  }
  return ParseArcSidebar(json);
}

void Count(const std::vector<ArcNode>& nodes, int& folders, int& pinned) {
  for (const ArcNode& node : nodes) {
    if (node.is_folder) {
      ++folders;
      Count(node.children, folders, pinned);
    } else {
      ++pinned;
    }
  }
}

}  // namespace

class ArcImportPageHandler::Batch : public base::RefCounted<Batch> {
 public:
  explicit Batch(ImportCallback callback) : callback_(std::move(callback)) {}

  void Add(const std::u16string& name, const ImportCounts& counts) {
    lines_.push_back(base::StrCat(
        {base::UTF16ToUTF8(name), ": ", base::NumberToString(counts.folders),
         " папок, ", base::NumberToString(counts.pinned), " pinned, ",
         base::NumberToString(counts.essentials), " essentials"}));
    if (counts.skipped) {
      lines_.back() += base::StrCat(
          {", уже были: ", base::NumberToString(counts.skipped)});
    }
  }

 private:
  friend base::RefCounted<Batch>;

  ~Batch() {
    std::string summary;
    for (const std::string& line : lines_) {
      summary += line + "\n";
    }
    std::move(callback_).Run(summary);
  }

  ImportCallback callback_;
  std::vector<std::string> lines_;
};

ArcImportPageHandler::Wait::Wait() = default;
ArcImportPageHandler::Wait::Wait(Wait&&) = default;
ArcImportPageHandler::Wait& ArcImportPageHandler::Wait::operator=(Wait&&) =
    default;
ArcImportPageHandler::Wait::~Wait() = default;

ArcImportPageHandler::ArcImportPageHandler(
    mojo::PendingReceiver<arc_import::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ArcImportPageHandler::~ArcImportPageHandler() = default;

void ArcImportPageHandler::GetArcSidebar(GetArcSidebarCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadArcSidebar, ArcSidebarPath()),
      base::BindOnce(&ArcImportPageHandler::OnSidebarRead,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcImportPageHandler::OnSidebarRead(GetArcSidebarCallback callback,
                                         std::optional<ArcSidebar> sidebar) {
  sidebar_ = std::move(sidebar);
  std::vector<arc_import::mojom::TargetPtr> targets;
  const std::string current = SpaceController::IdOf(profile_);
  for (const SpaceController::Space& space :
       SpaceController::Get()->GetSpaces()) {
    targets.push_back(arc_import::mojom::Target::New(
        space.id, base::UTF16ToUTF8(space.name), space.icon,
        space.id == current));
  }
  std::vector<arc_import::mojom::ArcSpacePtr> spaces;
  if (sidebar_) {
    for (const ArcSpace& space : sidebar_->spaces) {
      int folders = 0;
      int pinned = 0;
      Count(space.pinned, folders, pinned);
      auto essentials = sidebar_->essentials.find(space.arc_profile);
      spaces.push_back(arc_import::mojom::ArcSpace::New(
          space.arc_id, space.title, space.icon, folders, pinned,
          essentials == sidebar_->essentials.end()
              ? 0
              : static_cast<int>(essentials->second.size())));
    }
  }
  std::move(callback).Run(sidebar_.has_value(), std::move(spaces),
                          std::move(targets));
}

void ArcImportPageHandler::Import(
    std::vector<arc_import::mojom::ChoicePtr> choices,
    ImportCallback callback) {
  auto batch = base::MakeRefCounted<Batch>(std::move(callback));
  if (!sidebar_) {
    return;
  }
  // Arc spaces per existing Cromarc space; each "new" one gets its own space.
  std::map<std::string, std::vector<const ArcSpace*>> by_target;
  std::vector<const ArcSpace*> new_spaces;
  for (const auto& choice : choices) {
    for (const ArcSpace& space : sidebar_->spaces) {
      if (space.arc_id != choice->arc_id || choice->target.empty()) {
        continue;
      }
      if (choice->target == arc_import::mojom::kNewSpace) {
        new_spaces.push_back(&space);
      } else {
        by_target[choice->target].push_back(&space);
      }
    }
  }
  // Essentials belong to Arc profiles: each target gets those of the Arc
  // profiles its spaces were bound to.
  const auto essentials_for = [this](const std::vector<const ArcSpace*>& from) {
    std::vector<ArcNode> essentials;
    std::set<std::string> profiles;
    for (const ArcSpace* space : from) {
      auto it = sidebar_->essentials.find(space->arc_profile);
      if (it != sidebar_->essentials.end() &&
          profiles.insert(space->arc_profile).second) {
        essentials.insert(essentials.end(), it->second.begin(),
                          it->second.end());
      }
    }
    return essentials;
  };
  const auto copy = [](const std::vector<const ArcSpace*>& from) {
    std::vector<ArcSpace> spaces;
    for (const ArcSpace* space : from) {
      spaces.push_back(*space);
    }
    return spaces;
  };

  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  for (const SpaceController::Space& target :
       SpaceController::Get()->GetSpaces()) {
    auto it = by_target.find(target.id);
    if (it == by_target.end()) {
      continue;
    }
    auto import = base::BindOnce(&ArcImportPageHandler::ImportInto,
                                 weak_factory_.GetWeakPtr(), target.name,
                                 copy(it->second), essentials_for(it->second),
                                 batch);
    if (Profile* profile = profile_manager->GetProfileByPath(target.path)) {
      std::move(import).Run(profile);
    } else {
      profile_manager->LoadProfileByPath(target.path, /*incognito=*/false,
                                         std::move(import));
    }
  }
  for (const ArcSpace* space : new_spaces) {
    ProfileManager::CreateMultiProfileAsync(
        base::UTF8ToUTF16(space->title), profiles::GetPlaceholderAvatarIndex(),
        /*is_hidden=*/false,
        base::BindOnce(&ArcImportPageHandler::OnNewSpaceCreated,
                       weak_factory_.GetWeakPtr(), *space,
                       essentials_for({space}), batch));
  }
}

void ArcImportPageHandler::OnNewSpaceCreated(ArcSpace space,
                                             std::vector<ArcNode> essentials,
                                             scoped_refptr<Batch> batch,
                                             Profile* profile) {
  if (!profile) {
    return;
  }
  if (!space.icon.empty()) {
    profile->GetPrefs()->SetString(kSpaceIconPref, space.icon);
  }
  if (space.color) {
    ThemeServiceFactory::GetForProfile(profile)
        ->SetUserColorAndBrowserColorVariant(
            *space.color, ui::mojom::BrowserColorVariant::kTonalSpot);
  }
  const std::u16string name = base::UTF8ToUTF16(space.title);
  std::vector<ArcSpace> spaces;
  spaces.push_back(std::move(space));
  ImportInto(name, std::move(spaces), std::move(essentials), std::move(batch),
             profile);
}

void ArcImportPageHandler::ImportInto(std::u16string name,
                                      std::vector<ArcSpace> spaces,
                                      std::vector<ArcNode> essentials,
                                      scoped_refptr<Batch> batch,
                                      Profile* profile) {
  TreeService* const service = profile ? TreeService::Get(profile) : nullptr;
  if (!service) {
    return;
  }
  base::OnceClosure import = base::BindOnce(
      [](TreeService* service, std::u16string name,
         std::vector<ArcSpace> spaces, std::vector<ArcNode> essentials,
         scoped_refptr<Batch> batch) {
        std::vector<const ArcSpace*> pointers;
        for (const ArcSpace& space : spaces) {
          pointers.push_back(&space);
        }
        batch->Add(name, ImportIntoTree(pointers, essentials, service->tree()));
      },
      base::Unretained(service), std::move(name), std::move(spaces),
      std::move(essentials), std::move(batch));
  if (service->loaded()) {
    std::move(import).Run();
    return;
  }
  const int id = next_wait_id_++;
  Wait& wait = waits_[id];
  wait.service = service;
  wait.done = std::move(import);
  wait.subscription = service->RegisterOnChanged(
      base::BindRepeating(&ArcImportPageHandler::OnTreeChanged,
                          weak_factory_.GetWeakPtr(), id));
}

void ArcImportPageHandler::OnTreeChanged(int wait_id) {
  auto it = waits_.find(wait_id);
  if (it == waits_.end() || !it->second.service->loaded()) {
    return;
  }
  base::OnceClosure done = std::move(it->second.done);
  waits_.erase(it);
  std::move(done).Run();
}

}  // namespace cromarc
