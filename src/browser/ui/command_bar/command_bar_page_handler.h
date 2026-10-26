// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_PAGE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/callback_list.h"
#include "cromarc/browser/archive/archive.h"
#include "cromarc/browser/ports/omnibox.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ui/command_bar/command_bar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BrowserWindowInterface;

namespace cromarc {

class ArchiveService;
class TreeService;

// Searches for the command bar of `browser`: its tabs, the space's pinned
// items, the omnibox (history, bookmarks, search, typed URL) and the archive.
class CommandBarPageHandler : public command_bar::mojom::PageHandler {
 public:
  CommandBarPageHandler(
      mojo::PendingReceiver<command_bar::mojom::PageHandler> receiver,
      mojo::PendingRemote<command_bar::mojom::Page> page,
      BrowserWindowInterface* browser,
      TreeService* tree_service,
      ArchiveService* archive_service,
      Tabs& tabs,
      std::unique_ptr<Omnibox> omnibox);
  CommandBarPageHandler(const CommandBarPageHandler&) = delete;
  CommandBarPageHandler& operator=(const CommandBarPageHandler&) = delete;
  ~CommandBarPageHandler() override;

  void NotifyShown(const std::u16string& text);

  // command_bar::mojom::PageHandler:
  void Query(const std::string& text) override;
  void Open(command_bar::mojom::ResultPtr result) override;
  void Close() override;

 private:
  void OnArchiveResults(std::u16string text, std::vector<ArchiveEntry> entries);
  void Publish();

  mojo::Receiver<command_bar::mojom::PageHandler> receiver_;
  mojo::Remote<command_bar::mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TreeService> tree_service_;
  raw_ptr<ArchiveService> archive_service_;
  const raw_ref<Tabs> tabs_;
  const std::unique_ptr<Omnibox> omnibox_;
  base::CallbackListSubscription omnibox_changed_;

  std::u16string text_;
  // Results of the current query, per source.
  std::vector<command_bar::mojom::ResultPtr> tab_results_;
  std::vector<command_bar::mojom::ResultPtr> pinned_;
  std::vector<ArchiveEntry> archive_;
  std::map<std::string, TabId> tab_ids_;

  base::WeakPtrFactory<CommandBarPageHandler> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_COMMAND_BAR_COMMAND_BAR_PAGE_HANDLER_H_
