// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_MEDIA_H_
#define CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_MEDIA_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"

class Profile;

namespace cromarc {

// The Library's Media (zen:library/sections/ZenLibraryMediaSection.mjs, its
// scanner nsZenLibraryFiles): pictures and videos found in a few folders,
// newest first. The page knows a file by a number only: its thumbnail and
// the file itself are served under the sidebar's page by that number, never
// by a path the page names.
class LibraryMedia {
 public:
  using ScanCallback =
      base::OnceCallback<void(const std::vector<std::string>& roots,
                              std::vector<mojom::MediaItemPtr> items)>;

  // Zen's named folders ("downloads", "documents", "desktop", "pictures",
  // "music", "videos") and absolute paths; ones not found are passed over.
  static void Scan(const std::vector<std::string>& folders,
                   ScanCallback callback);
  static void Do(Profile* profile, uint32_t id, mojom::MediaCommand command);
  // What the system opens the file with, "" when it cannot tell.
  static std::string OpenerName(uint32_t id);

  // `media-thumb/<id>.png`, `media-large/<id>.png` (QuickLook's picture of
  // the file, small and big) and `media-file/<id>.<png|mp4>` (the file).
  static bool IsMediaPath(const std::string& path);
  static void Serve(const std::string& path,
                    content::WebUIDataSource::GotDataCallback callback);
};

// QuickLook (library_media_mac.mm): a PNG of `path` about `size` points
// wide, or null.
void GenerateThumbnail(
    const base::FilePath& path,
    int size,
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)> callback);
std::string AppNameToOpen(const base::FilePath& path);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_LIBRARY_MEDIA_H_
