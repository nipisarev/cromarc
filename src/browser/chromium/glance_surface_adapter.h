// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_GLANCE_SURFACE_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_GLANCE_SURFACE_ADAPTER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "cromarc/browser/ports/glance_surface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

class BrowserView;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace cromarc {

// GlanceSurface on the browser view: the page area lays the active tab out
// as a card (hook H11), the frame paints the parent's picture under it (hook
// H14), and child widgets carry the animations and the buttons.
class GlanceSurfaceAdapter : public GlanceSurface {
 public:
  GlanceSurfaceAdapter();
  GlanceSurfaceAdapter(const GlanceSurfaceAdapter&) = delete;
  GlanceSurfaceAdapter& operator=(const GlanceSurfaceAdapter&) = delete;
  ~GlanceSurfaceAdapter() override;

  // The card the page takes in `space` while `browser_view` shows a glance.
  std::optional<gfx::Rect> CardIn(BrowserView* browser_view,
                                  const gfx::Rect& space);
  // Under the card: the parent's picture, dimmed.
  void PaintBackdrop(gfx::Canvas* canvas, BrowserView* browser_view);

  // GlanceSurface:
  void Snapshot(TabId tab) override;
  void Forget(TabId tab) override;
  void Show(BrowserWindowInterface* window,
            TabId glance,
            TabId parent,
            std::optional<gfx::Point> origin,
            Client* client) override;
  void Hide(BrowserWindowInterface* window,
            bool animate,
            base::OnceClosure done) override;
  void AskToConfirmClose(BrowserWindowInterface* window,
                         base::TimeDelta duration) override;

 private:
  class Shown;

  const gfx::ImageSkia* PictureOf(TabId tab) const;

  std::map<BrowserWindowInterface*, std::unique_ptr<Shown>> shown_;
  std::map<TabId, gfx::ImageSkia> pictures_;
  base::WeakPtrFactory<GlanceSurfaceAdapter> weak_factory_{this};
};

GlanceSurfaceAdapter& ChromiumGlanceSurface();

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_GLANCE_SURFACE_ADAPTER_H_
