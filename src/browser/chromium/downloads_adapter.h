// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_DOWNLOADS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_DOWNLOADS_ADAPTER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "content/public/browser/web_ui_data_source.h"
#include "cromarc/browser/ports/library.h"
#include "cromarc/browser/ui/sidebar/sidebar.mojom.h"

class BrowserWindowInterface;
class Profile;

namespace cromarc {

// Downloads on what chrome://downloads uses: the profile's downloads for one
// window's Library and download stack
// (zen:library/sections/ZenLibraryDownloadsSection.mjs,
// zen:library/ZenLibraryWidget.sys.mjs).
class DownloadsAdapter : public Downloads,
                         public download::AllDownloadItemNotifier::Observer {
 public:
  // `on_changed` runs, at most four times a second, after a download came,
  // changed or went.
  DownloadsAdapter(BrowserWindowInterface* browser,
                   base::RepeatingClosure on_changed);
  DownloadsAdapter(const DownloadsAdapter&) = delete;
  DownloadsAdapter& operator=(const DownloadsAdapter&) = delete;
  ~DownloadsAdapter() override;

  // Downloads:
  std::vector<mojom::DownloadPtr> List() const override;
  void Do(uint32_t id, mojom::DownloadCommand command) override;
  void Clear() override;

  // The preview of a finished picture, `download-preview/<id>.png` under
  // the sidebar's page: only a finished download's own file, and only a
  // picture the page can draw.
  static bool IsPreviewPath(const std::string& path);
  static void ServePreview(Profile* profile,
                           const std::string& path,
                           content::WebUIDataSource::GotDataCallback callback);

  // download::AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

 private:
  content::DownloadManager* Manager() const;
  download::DownloadItem* Find(uint32_t id) const;
  void Retry(download::DownloadItem* item);
  void Drag(download::DownloadItem* item);
  void Changed();

  raw_ptr<BrowserWindowInterface> browser_;
  base::RepeatingClosure on_changed_;
  base::OneShotTimer changed_timer_;
  std::unique_ptr<download::AllDownloadItemNotifier> notifier_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_DOWNLOADS_ADAPTER_H_
