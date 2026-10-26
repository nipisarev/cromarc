// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_BROWSER_FRAME_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_BROWSER_FRAME_ADAPTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "cromarc/browser/ports/browser_frame.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

class BrowserView;
class BrowserWindowInterface;
class SkPath;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace cromarc {

// BrowserFrame on Chrome's BrowserView.
class BrowserFrameAdapter : public BrowserFrame {
 public:
  explicit BrowserFrameAdapter(BrowserWindowInterface* browser);
  BrowserFrameAdapter(const BrowserFrameAdapter&) = delete;
  BrowserFrameAdapter& operator=(const BrowserFrameAdapter&) = delete;
  ~BrowserFrameAdapter() override;

  // BrowserFrame:
  gfx::Rect BoundsInScreen() const override;
  void RepaintBackground() override;
  void UpdatePageCorners() override;
  base::CallbackListSubscription OnFullscreenChanged(
      base::RepeatingClosure callback) override;
  void AttachWebUI(content::WebContents* contents) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
};

// What the frame hooks call (patches/hooks/README.md).

// Hook H11, called by MultiContentsView's layout outside split view: the
// space round the page in a window with the sidebar, so the page is a card on
// the window's background. None in tab fullscreen.
gfx::Insets PageInsets(BrowserView* browser_view);

// Hook H18, from the browser view's layout: the page's corners, all round
// where it is a card, otherwise `chrome_corners`.
gfx::RoundedCornersF PageCorners(BrowserView* browser_view,
                                 const gfx::RoundedCornersF& chrome_corners);

// Hook H18, from ThemedBackground: true when `view` paints nothing, the space
// theme under it showing, as Zen's toolbox does (the bookmarks bar).
bool PaintsOnSpaceTheme(const views::View* view,
                        const BrowserView* browser_view);

// Hook H14: Zen draws the space theme (its gradient and grain) under the whole
// window and Chrome's frame, toolbar and page backgrounds stay clear over it,
// glass included. True for normal windows, the ones with the sidebar.
bool ThemesWindow(const BrowserView* browser_view);

// Hook H14, from the frame view's paint: the space theme over `bounds`.
void PaintWindowBackground(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           BrowserView* browser_view);

// Hook H15, from GlassFrameService: space windows keep their glass whether
// activated or not, so the window a space switch brings in has it already.
bool KeepsGlass(BrowserWindowInterface* browser);

// Hook H14, from CustomCornersBackground (toolbar, top container, tab strip
// region): true when Cromarc paints `view`'s background inside `path`
// instead, which is nothing but for the sidebar floating over the page in
// compact mode, which gets Zen's toolbar background.
bool PaintChromeBackground(gfx::Canvas* canvas,
                           const views::View* view,
                           const SkPath& path,
                           const BrowserView* browser_view);

// Hook H17, from CustomCornersBackground::SetUseBackgroundBlur(): the blur of
// what `view` floats over, nothing to leave it to Chrome. The sidebar out
// over the page in compact mode is acrylic (zen.theme.acrylic-sidebar).
std::optional<float> ChromeBackgroundBlur(const views::View* view,
                                          const BrowserView* browser_view);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_BROWSER_FRAME_ADAPTER_H_
