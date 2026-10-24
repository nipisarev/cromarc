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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_tabrestore.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "components/prefs/pref_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/arc_import/arc_import.h"
#include "cromarc/browser/spaces/space_controller.h"
#include "cromarc/browser/spaces/space_prefs.h"
#include "cromarc/browser/tree/tree_service.h"
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
void OpenTodayTabs(Profile* profile,
                   const std::vector<ArcSpace>& spaces,
                   TreeService& service,
                   ImportCounts& counts) {
  std::set<std::string> open;
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetProfile() != profile) {
      continue;
    }
    TabStripModel* tabs = window->GetTabStripModel();
    for (int i = 0; i < tabs->count(); ++i) {
      open.insert(UrlKey(tabs->GetWebContentsAt(i)->GetLastCommittedURL()));
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
    sessions::SerializedNavigationEntry entry;
    entry.set_virtual_url((*it)->url);
    entry.set_title((*it)->title);
    entry.set_timestamp(base::Time::Now());
    chrome::AddRestoredTab(
        window, base::span_from_ref(entry),
        /*tab_index=*/window->GetTabStripModel()->count(),
        /*selected_navigation=*/0, /*extension_app_id=*/std::string(),
        /*group=*/std::nullopt, /*select=*/false, /*pin=*/false,
        base::TimeTicks::Now(), base::Time::Now(),
        /*storage_namespace=*/nullptr, sessions::SerializedUserAgentOverride(),
        /*extra_data=*/{}, /*from_session_restore=*/false,
        /*is_active_browser=*/std::nullopt);
    ++counts.today;
  }
}

}  // namespace

class ArcImportPageHandler::Batch : public base::RefCounted<Batch> {
 public:
  explicit Batch(ImportCallback callback) : callback_(std::move(callback)) {}

  void Add(const std::u16string& name, const ImportCounts& counts) {
    lines_.push_back(base::StrCat(
        {base::UTF16ToUTF8(name), ": ", base::NumberToString(counts.folders),
         " папок, ", base::NumberToString(counts.pinned), " закреплённых, ",
         base::NumberToString(counts.essentials), " важных, ",
         base::NumberToString(counts.today), " в Today"}));
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
    Profile* profile,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_contents_(web_contents) {}

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
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.extensions = {{FILE_PATH_LITERAL("md")}};
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, u"Файл экспорта Arc",
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
      [](Profile* profile, TreeService* service, std::u16string name,
         std::vector<ArcSpace> spaces, std::vector<ArcNode> essentials,
         scoped_refptr<Batch> batch) {
        std::vector<const ArcSpace*> pointers;
        for (const ArcSpace& space : spaces) {
          pointers.push_back(&space);
        }
        ImportCounts counts =
            ImportIntoTree(pointers, essentials, service->tree());
        OpenTodayTabs(profile, spaces, *service, counts);
        batch->Add(name, counts);
      },
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
