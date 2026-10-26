// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_VERTICAL_STRIP_ADAPTER_H_
#define CROMARC_BROWSER_CHROMIUM_VERTICAL_STRIP_ADAPTER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "cromarc/browser/ports/vertical_strip.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class BrowserWindowInterface;
class VerticalTabStripRegionView;

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace cromarc {

// VerticalStrip on Chrome's VerticalTabStripStateController and
// VerticalTabStripRegionView.
class VerticalStripAdapter : public VerticalStrip,
                             public views::ViewObserver {
 public:
  // `inside`: a view in the strip's region, the one the sidebar goes in.
  VerticalStripAdapter(BrowserWindowInterface* browser, views::View* inside);
  VerticalStripAdapter(const VerticalStripAdapter&) = delete;
  VerticalStripAdapter& operator=(const VerticalStripAdapter&) = delete;
  ~VerticalStripAdapter() override;

  // VerticalStrip:
  bool IsShown() const override;
  bool IsCollapsed() const override;
  bool IsCollapsedOrCollapsing() const override;
  void RequestCollapse(bool collapse) override;
  int ExpandedWidth() const override;
  void SetExpandedWidth(int width) override;
  bool IsResizing() const override;
  base::CallbackListSubscription OnCollapseChanged(
      base::RepeatingClosure callback) override;
  base::CallbackListSubscription OnResizeEnded(
      base::RepeatingClosure callback) override;
  std::optional<gfx::Rect> BoundsInScreen() const override;
  std::unique_ptr<Hold> KeepOut() override;

  // views::ViewObserver (the region):
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  tabs::VerticalTabStripStateController* controller() const;
  VerticalTabStripRegionView* Region() const;

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<views::View> inside_;
  base::ScopedObservation<views::View, views::ViewObserver> region_observation_{
      this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_VERTICAL_STRIP_ADAPTER_H_
