// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/library_ports_adapter.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/platform_util.h"  // nogncheck: //chrome/browser links //cromarc
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "cromarc/browser/chromium/downloads_adapter.h"
#include "cromarc/browser/chromium/history_adapter.h"

namespace cromarc {

LibraryPortsAdapter::LibraryPortsAdapter() = default;
LibraryPortsAdapter::~LibraryPortsAdapter() = default;

std::unique_ptr<LibraryHistory> LibraryPortsAdapter::HistoryOf(
    BrowserWindowInterface* window) {
  return std::make_unique<HistoryAdapter>(window->GetProfile());
}

std::unique_ptr<Downloads> LibraryPortsAdapter::DownloadsOf(
    BrowserWindowInterface* window,
    base::RepeatingClosure on_changed) {
  return std::make_unique<DownloadsAdapter>(window, std::move(on_changed));
}

ShellAdapter::ShellAdapter() = default;
ShellAdapter::~ShellAdapter() = default;

void ShellAdapter::OpenFile(Profile* profile, const base::FilePath& path) {
  platform_util::OpenItem(profile, path, platform_util::OPEN_FILE,
                          base::DoNothing());
}

void ShellAdapter::ShowInFolder(Profile* profile, const base::FilePath& path) {
  platform_util::ShowItemInFolder(profile, path);
}

LibraryPorts& ChromiumLibraryPorts() {
  static base::NoDestructor<LibraryPortsAdapter> library_ports;
  return *library_ports;
}

Shell& ChromiumShell() {
  static base::NoDestructor<ShellAdapter> shell;
  return *shell;
}

}  // namespace cromarc
