// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
#define CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace cromarc {

class BrowserFrame;
class CompactMode;
class Tabs;
class VerticalStrip;
class Windows;

// Hosts chrome://cromarc-sidebar.top-chrome for one browser window. Fills the
// space the native tab list had in the vertical tab strip region.
class SidebarView : public views::WebView {
  METADATA_HEADER(SidebarView, views::WebView)

 public:
  // What a window gives its sidebar, built by the window's adapter
  // (InstallSidebar(), hook H2).
  struct Host {
    Host();
    Host(Host&&);
    Host& operator=(Host&&);
    ~Host();

    raw_ptr<BrowserWindowInterface> browser;
    raw_ptr<content::BrowserContext> browser_context;
    raw_ptr<PrefService> prefs;
    std::unique_ptr<BrowserFrame> frame;
    std::unique_ptr<VerticalStrip> strip;
    raw_ptr<Tabs> tabs;
    raw_ptr<Windows> windows;
  };

  explicit SidebarView(Host host);
  SidebarView(const SidebarView&) = delete;
  SidebarView& operator=(const SidebarView&) = delete;
  ~SidebarView() override;

  // The sidebar of `window` under `screen_point`, if any (for the space
  // switching input monitor).
  static SidebarView* FindAt(gfx::NativeWindow window,
                             const gfx::Point& screen_point);
  // The browser window `window` belongs to, if it has a sidebar.
  static BrowserWindowInterface* BrowserFor(gfx::NativeWindow window);
  static SidebarView* ForWindow(gfx::NativeWindow window);
  static SidebarView* ForBrowser(BrowserWindowInterface* browser);
  // Cmd+S (Arc) and Ctrl+Alt+C (Zen), from the key monitor and the sidebar's
  // page. False for windows without a sidebar.
  static bool ToggleCompactMode(BrowserWindowInterface* browser);

  BrowserWindowInterface* browser() const { return browser_; }
  CompactMode* compact_mode() { return compact_mode_.get(); }
  VerticalStrip& strip() { return *strip_; }

  // Once the sidebar's page is up, the window's strip shares its state with
  // the other space windows' (SpaceController::ShareSidebarState()).
  void ShareSidebarState();

  // The theme being edited, shown under the window until it is saved or
  // dropped (nullopt).
  const std::optional<std::string>& theme_preview() const {
    return theme_preview_;
  }
  void SetThemePreview(std::optional<std::string> theme);
  // While switching: the other space's theme and how far the window's
  // background has faded toward it.
  struct Blend {
    std::string theme;
    float progress;
  };
  const std::optional<Blend>& theme_blend() const { return theme_blend_; }
  void SetThemeBlend(std::optional<Blend> blend);

  // While the Library is out: the width the window's sidebar column takes
  // for it (hook H16); 0 when it is shut.
  int library_width() const { return library_width_; }
  void SetLibraryWidth(int width);
  // While the Library is shut: how far the sidebar sits from the top of its
  // region (Chrome's row of top buttons) and from its bottom (Chrome's
  // padding). The Library takes both while it is out (hook H19); the page
  // keeps the sidebar under it still by as much. Empty while it is out.
  gfx::Insets ColumnInsets() const;
  // The room the window's controls take at the sidebar's top left (Zen's top
  // row leaves it).
  gfx::Size WindowButtons() const;
  // Chromium's back or forward history menu at `point_in_page`, a point in
  // the sidebar's page.
  void ShowNavigationHistory(bool forward, const gfx::Point& point_in_page);
  // Where the page leaves room for the extension buttons, in its coordinates;
  // kept in place as the sidebar moves, hidden while it has no width.
  void PlaceExtensions(const gfx::Rect& in_page);
  int ExtensionsWidth() const;
  base::CallbackListSubscription OnExtensionsWidthChanged(
      base::RepeatingClosure callback);

  // content::WebContentsDelegate (through views::WebView):
  // Browser shortcuts (Cmd+T, Cmd+W, ...) keep working while the sidebar has
  // focus.
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

  // True where the page leaves room that moves the window (hook H25).
  bool IsDragRegion(const gfx::Point& point_in_view) const;

  // content::WebContentsObserver (through views::WebView): the window has no
  // other tab UI, so a crashed sidebar loads again instead of a sad tab.
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 protected:
  // views::WebView:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

 private:
  void Reload();
  // The page's rounded corners: none in tab fullscreen.
  void ObserveFullscreen();

  raw_ptr<BrowserWindowInterface> browser_;
  const std::unique_ptr<BrowserFrame> frame_;
  const std::unique_ptr<VerticalStrip> strip_;
  const raw_ref<Tabs> tabs_;
  std::unique_ptr<CompactMode> compact_mode_;
  base::ScopedClosureRunner shared_sidebar_state_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  // Reloads within a minute of the first one, to stop a crash loop.
  base::TimeTicks first_reload_;
  int reloads_ = 0;
  base::CallbackListSubscription fullscreen_subscription_;
  SkRegion drag_region_;
  gfx::Rect extensions_in_page_;
  std::optional<std::string> theme_preview_;
  std::optional<Blend> theme_blend_;
  int library_width_ = 0;
  // The space theme pref, painted under the window.
  PrefChangeRegistrar theme_pref_;
  base::WeakPtrFactory<SidebarView> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_SIDEBAR_SIDEBAR_VIEW_H_
