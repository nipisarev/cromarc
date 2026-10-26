// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_PAGE_HANDLER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cromarc/browser/arc_import/arc_sidebar.h"
#include "cromarc/browser/ui/arc_import/arc_import.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace cromarc {

class Profiles;
class SelectFilePolicies;
class Tabs;
class Theme;
class TreeService;
class Windows;

// chrome://cromarc-import: reads Arc's sidebar, or the file Arc's Export Data
// writes, and brings the Arc spaces the user picks into Cromarc spaces,
// existing or new (design, UI section 12).
class ArcImportPageHandler : public arc_import::mojom::PageHandler,
                             public ui::SelectFileDialog::Listener {
 public:
  ArcImportPageHandler(
      mojo::PendingReceiver<arc_import::mojom::PageHandler> receiver,
      content::WebContents* web_contents,
      Profiles& profiles,
      Windows& windows,
      Tabs& tabs,
      Theme& theme,
      SelectFilePolicies& select_file_policies);
  ArcImportPageHandler(const ArcImportPageHandler&) = delete;
  ArcImportPageHandler& operator=(const ArcImportPageHandler&) = delete;
  ~ArcImportPageHandler() override;

  // arc_import::mojom::PageHandler:
  void GetArcSidebar(GetArcSidebarCallback callback) override;
  void ChooseExportFile(ChooseExportFileCallback callback) override;
  void Import(std::vector<arc_import::mojom::ChoicePtr> choices,
              ImportCallback callback) override;

 private:
  // One Import() call; answers the page when the last space is done.
  class Batch;
  struct Wait {
    Wait();
    Wait(Wait&&);
    Wait& operator=(Wait&&);
    ~Wait();

    raw_ptr<TreeService> service;
    base::OnceClosure done;
    base::CallbackListSubscription subscription;
  };

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void ReadExportFile(const base::FilePath& path);
  // Keeps `sidebar` for Import() and answers the page with its spaces.
  void OnSidebarRead(GetArcSidebarCallback callback,
                     std::optional<ArcSidebar> sidebar);
  void OnNewSpaceCreated(ArcSpace space,
                         std::vector<ArcNode> essentials,
                         scoped_refptr<Batch> batch,
                         Profile* profile);
  // Imports into `profile`'s tree once its bookmarks have loaded; nothing
  // when the profile failed to load.
  void ImportInto(std::u16string name,
                  std::vector<ArcSpace> spaces,
                  std::vector<ArcNode> essentials,
                  scoped_refptr<Batch> batch,
                  Profile* profile);
  void OnTreeChanged(int wait_id);

  mojo::Receiver<arc_import::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  const raw_ref<Profiles> profiles_;
  const raw_ref<Windows> windows_;
  const raw_ref<Tabs> tabs_;
  const raw_ref<Theme> theme_;
  const raw_ref<SelectFilePolicies> select_file_policies_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  ChooseExportFileCallback export_callback_;
  std::optional<ArcSidebar> sidebar_;
  std::map<int, Wait> waits_;
  int next_wait_id_ = 0;
  base::WeakPtrFactory<ArcImportPageHandler> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_ARC_IMPORT_ARC_IMPORT_PAGE_HANDLER_H_
