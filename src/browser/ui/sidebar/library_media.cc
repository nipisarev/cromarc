// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/library_media.h"

#include <sys/stat.h>

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/platform_util.h"  // nogncheck: //chrome/browser links //cromarc
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace cromarc {

namespace {

// ZenLibraryMediaSection's MAX_DEPTH, MAX_FOLDERS and MAX_ITEMS.
constexpr int kMaxDepth = 2;
constexpr size_t kMaxFolders = 60;
constexpr size_t kMaxItems = 300;
// A file is served whole; a bigger one opens in its app instead.
constexpr int64_t kMaxFileBytes = 150 * 1024 * 1024;
// Points: Zen's tiles are at least 110px, its preview most of the window.
constexpr int kThumbSize = 128;
constexpr int kLargeSize = 1200;

constexpr char kThumbPrefix[] = "media-thumb/";
constexpr char kLargePrefix[] = "media-large/";
constexpr char kFilePrefix[] = "media-file/";

// ZenLibraryUtils' MEDIA_EXTENSIONS.
constexpr auto kImageExtensions = base::MakeFixedFlatSet<std::string_view>(
    {"arw", "avif", "bmp",  "cr2", "cr3",  "dng", "gif",  "heic",
     "heif", "ico", "icns", "jpe", "jpeg", "jpg", "jxl",  "nef",
     "orf",  "png", "raf",  "rw2", "svg",  "tif", "tiff", "webp"});
constexpr auto kVideoExtensions = base::MakeFixedFlatSet<std::string_view>(
    {"3g2", "3gp", "avi", "flv",  "m2ts", "m4v", "mkv", "mov",
     "mp4", "mpe", "mpeg", "mpg", "mts",  "ogv", "webm", "wmv"});
// Pictures the page draws itself (Zen's canDrawThumbnail); the others show
// QuickLook's picture of them.
constexpr auto kDrawnExtensions = base::MakeFixedFlatSet<std::string_view>(
    {"avif", "bmp", "gif", "ico", "jpe", "jpeg", "jpg", "png", "svg",
     "webp"});

std::string ExtensionOf(const base::FilePath& path) {
  std::string extension = path.FinalExtension();
  if (!extension.empty()) {
    extension.erase(0, 1);
  }
  return base::ToLowerASCII(extension);
}

std::string KindOf(const base::FilePath& path) {
  const std::string extension = ExtensionOf(path);
  if (kVideoExtensions.contains(extension)) {
    return "video";
  }
  return kDrawnExtensions.contains(extension) ? "image" : "file";
}

// The numbers the page knows files by, for as long as the browser runs.
struct Registry {
  std::map<uint32_t, base::FilePath> paths;
  std::map<base::FilePath, uint32_t> ids;
  uint32_t next = 1;
};

Registry& GetRegistry() {
  static base::NoDestructor<Registry> registry;
  return *registry;
}

uint32_t IdFor(const base::FilePath& path) {
  Registry& registry = GetRegistry();
  auto it = registry.ids.find(path);
  if (it != registry.ids.end()) {
    return it->second;
  }
  const uint32_t id = registry.next++;
  registry.ids[path] = id;
  registry.paths[id] = path;
  return id;
}

std::optional<base::FilePath> PathFor(uint32_t id) {
  Registry& registry = GetRegistry();
  auto it = registry.paths.find(id);
  if (it == registry.paths.end()) {
    return std::nullopt;
  }
  return it->second;
}

// Zen's FOLDERS, under the home folder; absolute paths as they are.
std::optional<base::FilePath> Resolve(const std::string& name) {
  const base::FilePath path(name);
  if (path.IsAbsolute()) {
    return path;
  }
  static constexpr std::pair<std::string_view, std::string_view> kFolders[] = {
      {"downloads", "Downloads"}, {"documents", "Documents"},
      {"desktop", "Desktop"},     {"pictures", "Pictures"},
      {"music", "Music"},         {"videos", "Movies"},
  };
  base::FilePath home;
  if (!base::PathService::Get(base::DIR_HOME, &home)) {
    return std::nullopt;
  }
  const std::string key = base::ToLowerASCII(name);
  for (const auto& [id, folder] : kFolders) {
    if (key == id) {
      return home.AppendASCII(folder);
    }
  }
  return std::nullopt;
}

struct Found {
  base::FilePath path;
  base::FilePath root;
  base::Time modified;
  int64_t size = 0;
  bool placeholder = false;
};

// nsZenLibraryFiles::ScanMedia: breadth first, two folders deep, sixty
// folders at most; the newest three hundred files.
std::vector<Found> ScanOnWorker(std::vector<base::FilePath> roots) {
  std::vector<Found> found;
  size_t folders = 0;
  for (const base::FilePath& root : roots) {
    std::deque<std::pair<base::FilePath, int>> queue{{root, 0}};
    while (!queue.empty() && folders < kMaxFolders) {
      const auto [folder, depth] = queue.front();
      queue.pop_front();
      ++folders;
      base::FileEnumerator files(
          folder, /*recursive=*/false,
          base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
      for (base::FilePath path = files.Next(); !path.empty();
           path = files.Next()) {
        if (path.BaseName().value().starts_with('.')) {
          continue;
        }
        const base::FileEnumerator::FileInfo info = files.GetInfo();
        if (info.IsDirectory()) {
          // Packages (an .app, a Photos library) are files to Finder.
          if (depth + 1 <= kMaxDepth && path.FinalExtension().empty()) {
            queue.emplace_back(path, depth + 1);
          }
          continue;
        }
        const std::string extension = ExtensionOf(path);
        if (!kImageExtensions.contains(extension) &&
            !kVideoExtensions.contains(extension)) {
          continue;
        }
        Found file;
        file.path = path;
        file.root = root;
        file.modified = info.GetLastModifiedTime();
        file.size = info.GetSize();
        // In iCloud Drive and not here: reading it would bring it down.
        file.placeholder = (info.stat().st_flags & SF_DATALESS) != 0;
        found.push_back(std::move(file));
      }
    }
  }
  std::ranges::sort(found, std::greater<>(), &Found::modified);
  if (found.size() > kMaxItems) {
    found.resize(kMaxItems);
  }
  return found;
}

scoped_refptr<base::RefCountedMemory> ReadWhole(const base::FilePath& path) {
  std::optional<int64_t> size = base::GetFileSize(path);
  if (!size || *size > kMaxFileBytes) {
    return nullptr;
  }
  std::optional<std::vector<uint8_t>> bytes = base::ReadFileToBytes(path);
  if (!bytes) {
    return nullptr;
  }
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(*bytes));
}

// The number in `media-<what>/<id>.<ext>`.
std::optional<uint32_t> IdIn(std::string_view path, std::string_view prefix) {
  std::string_view rest = path.substr(prefix.size());
  const size_t dot = rest.find('.');
  uint32_t id = 0;
  if (dot == std::string_view::npos ||
      !base::StringToUint(rest.substr(0, dot), &id)) {
    return std::nullopt;
  }
  return id;
}

}  // namespace

// static
void LibraryMedia::Scan(const std::vector<std::string>& folders,
                        ScanCallback callback) {
  std::vector<base::FilePath> roots;
  for (const std::string& folder : folders) {
    if (std::optional<base::FilePath> root = Resolve(folder);
        root && !std::ranges::contains(roots, *root)) {
      roots.push_back(*root);
    }
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ScanOnWorker, roots),
      base::BindOnce(
          [](std::vector<base::FilePath> roots, ScanCallback callback,
             std::vector<Found> found) {
            std::vector<std::string> root_names;
            for (const base::FilePath& root : roots) {
              root_names.push_back(root.AsUTF8Unsafe());
            }
            std::vector<mojom::MediaItemPtr> items;
            for (const Found& file : found) {
              auto item = mojom::MediaItem::New();
              item->id = IdFor(file.path);
              item->folder = file.root.AsUTF8Unsafe();
              item->name = file.path.BaseName().AsUTF8Unsafe();
              item->kind = KindOf(file.path);
              item->modified = file.modified.InMillisecondsFSinceUnixEpoch();
              item->size = file.size;
              item->cloud_placeholder = file.placeholder;
              item->served_whole = file.size <= kMaxFileBytes;
              items.push_back(std::move(item));
            }
            std::move(callback).Run(root_names, std::move(items));
          },
          roots, std::move(callback)));
}

// static
void LibraryMedia::Do(Profile* profile,
                      uint32_t id,
                      mojom::MediaCommand command) {
  std::optional<base::FilePath> path = PathFor(id);
  if (!path) {
    return;
  }
  switch (command) {
    case mojom::MediaCommand::kOpen:
      platform_util::OpenItem(profile, *path, platform_util::OPEN_FILE,
                              base::DoNothing());
      break;
    case mojom::MediaCommand::kShow:
      platform_util::ShowItemInFolder(profile, *path);
      break;
    case mojom::MediaCommand::kCopy:
      ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
          .WriteFilenames(ui::FileInfosToURIList(
              {ui::FileInfo(*path, base::FilePath())}));
      break;
  }
}

// static
std::string LibraryMedia::OpenerName(uint32_t id) {
  std::optional<base::FilePath> path = PathFor(id);
  return path ? AppNameToOpen(*path) : std::string();
}

// static
bool LibraryMedia::IsMediaPath(const std::string& path) {
  return path.starts_with(kThumbPrefix) || path.starts_with(kLargePrefix) ||
         path.starts_with(kFilePrefix);
}

// static
void LibraryMedia::Serve(const std::string& path,
                         content::WebUIDataSource::GotDataCallback callback) {
  const bool thumb = path.starts_with(kThumbPrefix);
  const bool large = path.starts_with(kLargePrefix);
  const std::string_view prefix =
      thumb ? kThumbPrefix : (large ? kLargePrefix : kFilePrefix);
  std::optional<uint32_t> id = IdIn(path, prefix);
  std::optional<base::FilePath> file = id ? PathFor(*id) : std::nullopt;
  if (!file) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (thumb || large) {
    GenerateThumbnail(*file, thumb ? kThumbSize : kLargeSize,
                      std::move(callback));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadWhole, *file), std::move(callback));
}

}  // namespace cromarc
