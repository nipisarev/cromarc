// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_PAGE_HANDLER_H_
#define CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_PAGE_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "cromarc/browser/ui/sidebar/theme_picker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BrowserWindowInterface;

namespace cromarc {

class Windows;

// The theme picker's page in `browser`'s ThemePicker: what it edits is
// previewed under the window if the space is the window's own (Zen rebuilds
// the background only for the space a window shows), and saved when the
// panel closes (handlePanelClose).
class ThemePickerPageHandler : public theme_picker::mojom::PageHandler {
 public:
  ThemePickerPageHandler(
      mojo::PendingReceiver<theme_picker::mojom::PageHandler> receiver,
      mojo::PendingRemote<theme_picker::mojom::Page> page,
      BrowserWindowInterface* browser,
      Windows& windows);
  ThemePickerPageHandler(const ThemePickerPageHandler&) = delete;
  ThemePickerPageHandler& operator=(const ThemePickerPageHandler&) = delete;
  ~ThemePickerPageHandler() override;

  // The panel opened for `space_id`.
  void NotifyOpened(const std::string& space_id);
  // The panel closed: saves what was previewed and drops the preview.
  void Commit();

  // theme_picker::mojom::PageHandler:
  void Preview(const std::string& theme) override;
  void SetScheme(const std::string& scheme) override;
  void PlayHapticFeedback() override;
  void Close() override;
  void SetHeight(int32_t height) override;

 private:
  bool EditsOwnSpace() const;
  void SetWindowPreview(std::optional<std::string> theme);

  mojo::Receiver<theme_picker::mojom::PageHandler> receiver_;
  mojo::Remote<theme_picker::mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  const raw_ref<Windows> windows_;
  std::string space_id_;
  std::optional<std::string> edited_;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_THEME_PICKER_PAGE_HANDLER_H_
