// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/library_downloads.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/download/drag_download_item.h"  // nogncheck: as above
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/ui/sidebar/sidebar_view.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"

namespace cromarc {

namespace {

constexpr base::TimeDelta kChangedDelay = base::Milliseconds(250);
constexpr char kPreviewPrefix[] = "download-preview/";
// Pictures the page draws, as ZenLibraryUtils' canDrawThumbnail leaves out
// what Firefox cannot. Each is served as .png, the data source knowing few
// types; the image decoder goes by what the file holds.
constexpr auto kPreviewExtensions = base::MakeFixedFlatSet<std::string_view>(
    {"avif", "bmp", "gif", "ico", "jpeg", "jpg", "png", "webp"});
// A preview is the file itself; a bigger one is left to its icon.
constexpr int64_t kMaxPreviewBytes = 32 * 1024 * 1024;

mojom::DownloadState StateOf(const download::DownloadItem& item) {
  switch (item.GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      return mojom::DownloadState::kInProgress;
    case download::DownloadItem::COMPLETE:
      return mojom::DownloadState::kComplete;
    case download::DownloadItem::CANCELLED:
      return mojom::DownloadState::kCancelled;
    case download::DownloadItem::INTERRUPTED:
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      return mojom::DownloadState::kInterrupted;
  }
}

// The lowercase extension of `path`, without its dot.
std::string ExtensionOf(const base::FilePath& path) {
  std::string extension = path.FinalExtension();
  if (!extension.empty()) {
    extension.erase(0, 1);
  }
  return base::ToLowerASCII(extension);
}

bool HasPreview(const download::DownloadItem& item) {
  return item.GetState() == download::DownloadItem::COMPLETE &&
         !item.GetFileExternallyRemoved() &&
         kPreviewExtensions.contains(ExtensionOf(item.GetTargetFilePath()));
}

std::string PreviewPathOf(const download::DownloadItem& item) {
  return HasPreview(item) ? base::StrCat({kPreviewPrefix,
                                          base::NumberToString(item.GetId()),
                                          ".png"})
                          : std::string();
}

scoped_refptr<base::RefCountedMemory> ReadPreview(const base::FilePath& path) {
  std::optional<int64_t> size = base::GetFileSize(path);
  if (!size || *size > kMaxPreviewBytes) {
    return nullptr;
  }
  std::optional<std::vector<uint8_t>> bytes = base::ReadFileToBytes(path);
  if (!bytes) {
    return nullptr;
  }
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(*bytes));
}

}  // namespace

LibraryDownloads::LibraryDownloads(BrowserWindowInterface* browser,
                                   base::RepeatingClosure on_changed)
    : browser_(browser), on_changed_(std::move(on_changed)) {
  notifier_ =
      std::make_unique<download::AllDownloadItemNotifier>(Manager(), this);
}

LibraryDownloads::~LibraryDownloads() = default;

content::DownloadManager* LibraryDownloads::Manager() const {
  return browser_->GetProfile()->GetDownloadManager();
}

download::DownloadItem* LibraryDownloads::Find(uint32_t id) const {
  return Manager()->GetDownload(id);
}

std::vector<mojom::DownloadPtr> LibraryDownloads::List() const {
  content::DownloadManager::DownloadVector items;
  Manager()->GetAllDownloads(&items);
  std::ranges::stable_sort(items, {}, [](const auto& item) {
    return item->GetStartTime();
  });
  std::vector<mojom::DownloadPtr> downloads;
  for (download::DownloadItem* item : items) {
    if (item->IsTransient() || item->IsTemporary() ||
        !DownloadItemModel(item).ShouldShowInUi()) {
      continue;
    }
    auto download = mojom::Download::New();
    download->id = item->GetId();
    const base::FilePath name = item->GetFileNameToReportUser();
    download->file_name =
        name.empty() ? item->GetURL().spec() : name.AsUTF8Unsafe();
    download->path = item->GetTargetFilePath().AsUTF8Unsafe();
    download->url = item->GetURL();
    download->state = StateOf(*item);
    download->paused = item->IsPaused();
    download->missing = item->GetState() == download::DownloadItem::COMPLETE &&
                        item->GetFileExternallyRemoved();
    download->dangerous =
        !item->IsDone() && (item->IsDangerous() || item->IsInsecure());
    download->can_resume = item->CanResume();
    download->open_when_complete = item->GetOpenWhenComplete();
    download->received_bytes = item->GetReceivedBytes();
    download->total_bytes =
        item->GetTotalBytes() > 0 ? item->GetTotalBytes() : -1;
    download->speed = item->CurrentSpeed();
    base::TimeDelta left;
    download->time_left = item->TimeRemaining(&left) ? left.InSeconds() : -1;
    download->start_time =
        item->GetStartTime().InMillisecondsFSinceUnixEpoch();
    download->end_time =
        item->GetEndTime().is_null()
            ? 0
            : item->GetEndTime().InMillisecondsFSinceUnixEpoch();
    download->preview_url = PreviewPathOf(*item);
    downloads.push_back(std::move(download));
  }
  return downloads;
}

void LibraryDownloads::Do(uint32_t id, mojom::DownloadCommand command) {
  download::DownloadItem* item = Find(id);
  if (!item) {
    return;
  }
  switch (command) {
    case mojom::DownloadCommand::kOpen:
      if (item->GetState() == download::DownloadItem::COMPLETE) {
        item->OpenDownload();
      } else if (item->GetState() == download::DownloadItem::IN_PROGRESS &&
                 !item->IsDangerous() && !item->IsInsecure()) {
        // Zen's open-when-done, as the downloads panel has it.
        item->SetOpenWhenComplete(!item->GetOpenWhenComplete());
        Changed();
      }
      break;
    case mojom::DownloadCommand::kShow:
      item->ShowDownloadInShell();
      break;
    case mojom::DownloadCommand::kCancel:
      item->Cancel(/*user_cancel=*/true);
      break;
    case mojom::DownloadCommand::kPause:
      item->Pause();
      break;
    case mojom::DownloadCommand::kResume:
      item->Resume(/*user_resume=*/true);
      break;
    case mojom::DownloadCommand::kRetry:
      Retry(item);
      break;
    case mojom::DownloadCommand::kHide:
      if (item->GetState() != download::DownloadItem::IN_PROGRESS) {
        item->Remove();
      }
      break;
    case mojom::DownloadCommand::kDeleteFile:
      if (item->GetState() == download::DownloadItem::COMPLETE) {
        item->DeleteFile(base::DoNothing());
      } else if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
        item->Cancel(/*user_cancel=*/true);
      }
      break;
    case mojom::DownloadCommand::kCopyFile:
      if (item->GetState() == download::DownloadItem::COMPLETE &&
          !item->GetFileExternallyRemoved()) {
        ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
            .WriteFilenames(ui::FileInfosToURIList(
                {ui::FileInfo(item->GetTargetFilePath(), base::FilePath())}));
      }
      break;
    case mojom::DownloadCommand::kCopyLink:
      ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
          .WriteText(base::UTF8ToUTF16(item->GetURL().spec()));
      break;
    case mojom::DownloadCommand::kKeepDangerous:
      if (item->IsInsecure()) {
        item->ValidateInsecureDownload();
      } else if (item->IsDangerous()) {
        item->ValidateDangerousDownload();
      }
      break;
    case mojom::DownloadCommand::kDiscardDangerous:
      item->Remove();
      break;
    case mojom::DownloadCommand::kDrag:
      Drag(item);
      break;
  }
}

void LibraryDownloads::Clear() {
  content::DownloadManager::DownloadVector items;
  Manager()->GetAllDownloads(&items);
  for (download::DownloadItem* item : items) {
    if (item->GetState() != download::DownloadItem::IN_PROGRESS &&
        !item->IsTransient()) {
      item->Remove();
    }
  }
}

// chrome://downloads' Retry: a download that can pick up where it stopped
// does; any other starts again, as the request that began it.
void LibraryDownloads::Retry(download::DownloadItem* item) {
  if (item->CanResume()) {
    item->Resume(/*user_resume=*/true);
    return;
  }
  SidebarView* sidebar = SidebarView::ForBrowser(browser_);
  content::WebContents* contents = sidebar ? sidebar->GetWebContents() : nullptr;
  if (!contents) {
    return;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("cromarc_library_download_retry", R"(
        semantics {
          sender: "Cromarc Library"
          description: "Retrying a download."
          trigger:
            "The user retries a cancelled or failed download in the Library "
            "or its download stack."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Only triggered by user request."
          policy_exception_justification: "Not implemented."
        })");
  auto params = contents->GetPrimaryMainFrame()->CreateDownloadUrlParameters(
      item->GetURL(), traffic_annotation);
  params->set_content_initiated(true);
  params->set_initiator(item->GetRequestInitiator());
  params->set_download_source(download::DownloadSource::RETRY);
  Manager()->DownloadUrl(std::move(params));
}

// chrome://downloads' Drag: the file as the system drags it, with its icon.
void LibraryDownloads::Drag(download::DownloadItem* item) {
  SidebarView* sidebar = SidebarView::ForBrowser(browser_);
  content::WebContents* contents = sidebar ? sidebar->GetWebContents() : nullptr;
  if (!contents || item->GetState() != download::DownloadItem::COMPLETE ||
      item->GetFileExternallyRemoved()) {
    return;
  }
  gfx::NativeView view = contents->GetNativeView();
  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      item->GetTargetFilePath(), IconLoader::NORMAL,
      display::Screen::Get()->GetPreferredScaleFactorForView(view).value_or(
          1.0f));
  // The drag runs a nested loop; tasks go on meanwhile.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  DragDownloadItem(item, icon, view);
}

// static
bool LibraryDownloads::IsPreviewPath(const std::string& path) {
  return path.starts_with(kPreviewPrefix);
}

// static
void LibraryDownloads::ServePreview(
    Profile* profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  const std::string_view name = std::string_view(path).substr(
      std::size(kPreviewPrefix) - 1);
  const size_t dot = name.find('.');
  uint32_t id = 0;
  download::DownloadItem* item =
      dot != std::string_view::npos &&
              base::StringToUint(name.substr(0, dot), &id)
          ? profile->GetDownloadManager()->GetDownload(id)
          : nullptr;
  if (!item || PreviewPathOf(*item) != path) {
    std::move(callback).Run(nullptr);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadPreview, item->GetTargetFilePath()),
      std::move(callback));
}

void LibraryDownloads::OnDownloadCreated(content::DownloadManager* manager,
                                         download::DownloadItem* item) {
  Changed();
}

void LibraryDownloads::OnDownloadUpdated(content::DownloadManager* manager,
                                         download::DownloadItem* item) {
  Changed();
}

void LibraryDownloads::OnDownloadRemoved(content::DownloadManager* manager,
                                         download::DownloadItem* item) {
  Changed();
}

void LibraryDownloads::Changed() {
  if (!changed_timer_.IsRunning()) {
    changed_timer_.Start(FROM_HERE, kChangedDelay, on_changed_);
  }
}

}  // namespace cromarc
