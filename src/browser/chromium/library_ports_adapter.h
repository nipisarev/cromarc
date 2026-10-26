// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_LIBRARY_PORTS_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_LIBRARY_PORTS_ADAPTER_H_

#include "cromarc/browser/ports/library.h"
#include "cromarc/browser/ports/shell.h"

namespace cromarc {

// LibraryPorts making the history and downloads adapters.
class LibraryPortsAdapter : public LibraryPorts {
 public:
  LibraryPortsAdapter();
  LibraryPortsAdapter(const LibraryPortsAdapter&) = delete;
  LibraryPortsAdapter& operator=(const LibraryPortsAdapter&) = delete;
  ~LibraryPortsAdapter() override;

  // LibraryPorts:
  std::unique_ptr<LibraryHistory> HistoryOf(
      BrowserWindowInterface* window) override;
  std::unique_ptr<Downloads> DownloadsOf(
      BrowserWindowInterface* window,
      base::RepeatingClosure on_changed) override;
};

// Shell on Chrome's platform_util.
class ShellAdapter : public Shell {
 public:
  ShellAdapter();
  ShellAdapter(const ShellAdapter&) = delete;
  ShellAdapter& operator=(const ShellAdapter&) = delete;
  ~ShellAdapter() override;

  // Shell:
  void OpenFile(Profile* profile, const base::FilePath& path) override;
  void ShowInFolder(Profile* profile, const base::FilePath& path) override;
};

// The process-wide ones, for SidebarUI.
LibraryPorts& ChromiumLibraryPorts();
Shell& ChromiumShell();

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_LIBRARY_PORTS_ADAPTER_H_
