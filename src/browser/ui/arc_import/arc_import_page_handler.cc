// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/arc_import/arc_import_page_handler.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/arc_import/arc_import.h"
#include "cromarc/browser/ports/profiles.h"
#include "cromarc/browser/ports/select_file_policies.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/theme.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/spaces/space_pref_names.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace cromarc {

namespace {

// Tests point the page at a fixture instead of the user's Arc data, and at
// an export file instead of the dialog.
constexpr char kArcSidebarSwitch[] = "cromarc-arc-sidebar";
constexpr char kArcExportSwitch[] = "cromarc-arc-export";

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

std::optional<ArcSidebar> ReadArcExport(const base::FilePath& path) {
  std::string markdown;
  if (!base::ReadFileToString(path, &markdown)) {
    return std::nullopt;
  }
  return ParseArcExport(markdown);
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

// Arc's Today tabs open in Today as restored tabs do, loading once selected,
// Arc's top one on top (Today lists the newest tab first). A URL pinned in
// the tree or open in the space already is left out.
void OpenTodayTabs(Windows& windows,
                   Tabs& tabs,
                   Profile* profile,
                   const std::vector<ArcSpace>& spaces,
                   TreeService& service,
                   ImportCounts& counts) {
  std::set<std::string> open;
  for (BrowserWindowInterface* window : windows.All()) {
    if (windows.ProfileOf(window) != profile) {
      continue;
    }
    for (TabId tab : tabs.List(window)) {
      open.insert(UrlKey(tabs.GetContents(tab)->GetLastCommittedURL()));
    }
  }
  std::vector<const ArcNode*> today;
  for (const ArcSpace& space : spaces) {
    for (const ArcNode& node : space.today) {
      if (service.FindItemByHomeUrl(node.url) ||
          !open.insert(UrlKey(node.url)).second) {
        ++counts.skipped;
        continue;
      }
      today.push_back(&node);
    }
  }
  if (today.empty()) {
    return;
  }
  BrowserWindowInterface* window =
      SpaceController::Get()->EnsureHiddenWindow(profile);
  if (!window) {
    return;
  }
  for (auto it = today.rbegin(); it != today.rend(); ++it) {
    tabs.AddUnloaded(window, (*it)->url, (*it)->title);
    ++counts.today;
  }
}

}  // namespace

class ArcImportPageHandler::Batch : public base::RefCounted<Batch> {
 public:
  explicit Batch(ImportCallback callback) : callback_(std::move(callback)) {}

  void Add(const std::u16string& name, const ImportCounts& counts) {
    lines_.push_back(base::UTF16ToUTF8(SummaryLine(name, counts)));
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
    content::WebContents* web_contents,
    Profiles& profiles,
    Windows& windows,
    Tabs& tabs,
    Theme& theme,
    SelectFilePolicies& select_file_policies)
    : receiver_(this, std::move(receiver)),
      profile_(profiles.FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      profiles_(profiles),
      windows_(windows),
      tabs_(tabs),
      theme_(theme),
      select_file_policies_(select_file_policies) {}

ArcImportPageHandler::~ArcImportPageHandler() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void ArcImportPageHandler::GetArcSidebar(GetArcSidebarCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadArcSidebar, ArcSidebarPath()),
      base::BindOnce(&ArcImportPageHandler::OnSidebarRead,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcImportPageHandler::ChooseExportFile(
    ChooseExportFileCallback callback) {
  if (export_callback_) {
    std::move(callback).Run(false, {}, {});
    return;
  }
  export_callback_ = std::move(callback);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kArcExportSwitch)) {
    ReadExportFile(command_line->GetSwitchValuePath(kArcExportSwitch));
    return;
  }
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, select_file_policies_->For(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.extensions = {{FILE_PATH_LITERAL("md")}};
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_CROMARC_ARC_IMPORT_EXPORT_FILE),
      base::GetHomeDir().Append("Downloads"), &file_types, 1,
      FILE_PATH_LITERAL("md"), web_contents_->GetTopLevelNativeWindow());
}

void ArcImportPageHandler::FileSelected(const ui::SelectedFileInfo& file,
                                        int index) {
  select_file_dialog_ = nullptr;
  ReadExportFile(file.path());
}

void ArcImportPageHandler::FileSelectionCanceled() {
  select_file_dialog_ = nullptr;
  std::move(export_callback_).Run(false, {}, {});
}

void ArcImportPageHandler::ReadExportFile(const base::FilePath& path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadArcExport, path),
      base::BindOnce(
          [](base::WeakPtr<ArcImportPageHandler> self,
             std::optional<ArcSidebar> sidebar) {
            if (!self) {
              return;
            }
            ChooseExportFileCallback callback =
                std::move(self->export_callback_);
            // Not Arc's export: the spaces read before stay.
            if (!sidebar) {
              std::move(callback).Run(false, {}, {});
              return;
            }
            self->OnSidebarRead(std::move(callback), std::move(sidebar));
          },
          weak_factory_.GetWeakPtr()));
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
              : static_cast<int>(essentials->second.size()),
          static_cast<int>(space.today.size())));
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
    if (Profile* profile = profiles_->GetLoaded(target.id)) {
      std::move(import).Run(profile);
    } else {
      profiles_->Load(target.id, std::move(import));
    }
  }
  for (const ArcSpace* space : new_spaces) {
    profiles_->Create(
        base::UTF8ToUTF16(space->title),
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
    profiles_->PrefsOf(profile)->SetString(kSpaceIconPref, space.icon);
  }
  if (space.color) {
    theme_->SetColor(profile, *space.color);
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
  // The ports outlive the import: they are the browser's.
  base::OnceClosure import = base::BindOnce(
      [](Windows* windows, Tabs* tabs, Profile* profile, TreeService* service,
         std::u16string name, std::vector<ArcSpace> spaces,
         std::vector<ArcNode> essentials, scoped_refptr<Batch> batch) {
        std::vector<const ArcSpace*> pointers;
        for (const ArcSpace& space : spaces) {
          pointers.push_back(&space);
        }
        ImportCounts counts =
            ImportIntoTree(pointers, essentials, service->tree());
        OpenTodayTabs(*windows, *tabs, profile, spaces, *service, counts);
        batch->Add(name, counts);
      },
      base::Unretained(&windows_.get()), base::Unretained(&tabs_.get()),
      base::Unretained(profile), base::Unretained(service), std::move(name),
      std::move(spaces), std::move(essentials), std::move(batch));
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
