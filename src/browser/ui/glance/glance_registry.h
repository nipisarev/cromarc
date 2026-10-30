// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_UI_GLANCE_GLANCE_REGISTRY_H_
#define CROMARC_BROWSER_UI_GLANCE_GLANCE_REGISTRY_H_

#include <map>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cromarc/browser/ports/glance_surface.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ui/glance/glance_rules.h"
#include "ui/gfx/geometry/point.h"

class BrowserWindowInterface;
class GURL;

namespace cromarc {

class AppLifecycle;
class Windows;

// Glances, as Zen's ZenGlanceManager has them: a glance is a tab of the tab it
// was opened from (its parent), one per parent. The sidebar does not list it;
// while it is the active tab its page shows as a card over its parent's.
// Selecting the parent shows its glance again; closing, archiving or putting
// the parent to sleep sends the glance to the archive with it.
class GlanceRegistry : public GlanceSurface::Client {
 public:
  // What the registry asks of each space.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual TabKind KindOf(TabId tab) = 0;
    // Records the glance's page in its space's archive.
    virtual void Archive(TabId glance, bool session_ending) = 0;
  };

  struct Entry {
    TabId glance;
    TabId parent;
  };

  // The browser's; on the Chromium ports.
  static GlanceRegistry* Get();

  GlanceRegistry(Tabs& tabs,
                 Windows& windows,
                 AppLifecycle& app_lifecycle,
                 GlanceSurface& surface,
                 Delegate& delegate);
  GlanceRegistry(const GlanceRegistry&) = delete;
  GlanceRegistry& operator=(const GlanceRegistry&) = delete;
  ~GlanceRegistry() override;

  // Follows `window`'s tabs: the tabs its pages open may be glances, and its
  // glances show and hide as its tabs come and go.
  void Watch(BrowserWindowInterface* window);

  // The next tab `parent` opens, active, is its glance, growing out of
  // `origin`. False when it cannot have one: it is a glance, or has one.
  bool Expect(TabId parent, std::optional<gfx::Point> origin);
  // The user asked for a tab (Cmd+click): the next tab `parent` opens is one,
  // whatever its site.
  void ExpectTab(TabId parent);
  // A navigation to `url` starts in `tab`: a tab an Essential or a pinned
  // tab opened before its address was known (window.open) may be its glance.
  void OnNavigationStarting(TabId tab, const GURL& url);

  // What `tab` is in its space's sidebar.
  TabKind KindOf(TabId tab);
  bool IsGlance(TabId tab) const;
  std::optional<TabId> GlanceOf(TabId parent) const;
  std::optional<TabId> ParentOf(TabId glance) const;
  std::vector<Entry> GlancesIn(BrowserWindowInterface* window) const;

  // The user closes it (its ✕, Esc, a click beside it, Cmd+W): it goes, as a
  // tab the user closes does, and its parent comes back.
  void Close(TabId glance);
  // It becomes an ordinary tab: first in Today when its parent is an
  // Essential or pinned, else right after its parent.
  void Expand(TabId glance);
  // Cmd+W and Cmd+O in `window`: true when its active tab is a glance, now
  // closing or expanded.
  bool CloseActive(BrowserWindowInterface* window);
  bool ExpandActive(BrowserWindowInterface* window);
  // A click on a tab's row or tile: its glance, when it has one (Zen's
  // getTabOrGlanceChild).
  void ActivateTabOrGlance(TabId tab);

  // Runs when a glance comes or goes.
  base::CallbackListSubscription RegisterOnChanged(
      base::RepeatingClosure callback);

  // GlanceSurface::Client:
  void CloseGlance(BrowserWindowInterface* window) override;
  void ExpandGlance(BrowserWindowInterface* window) override;
  // Esc closes it, as Zen's. With a text field focused the first Esc only
  // asks, and Esc again while the close button asks closes it (Zen's
  // #handleConfirmationTimeout).
  void EscapeGlance(BrowserWindowInterface* window, bool in_field) override;

 private:
  struct Glance {
    TabId parent;
    raw_ptr<BrowserWindowInterface> window = nullptr;
    // Where the first showing grows out of.
    std::optional<gfx::Point> origin;
    bool closing_by_user = false;
    // Until when its close button asks for a confirmation.
    base::TimeTicks confirm_until;
    base::CallbackListSubscription parent_closes;
    base::CallbackListSubscription closes;
  };
  struct Expectation {
    bool glance = true;
    std::optional<gfx::Point> origin;
    base::TimeTicks since;
  };
  struct Watched {
    base::CallbackListSubscription inserted;
    base::CallbackListSubscription active_changed;
    base::CallbackListSubscription closed;
  };

  void OnInserted(BrowserWindowInterface* window,
                  TabId tab,
                  std::optional<TabId> opener,
                  bool active);
  void OnActiveChanged(BrowserWindowInterface* window);
  void Mark(TabId tab,
            TabId parent,
            BrowserWindowInterface* window,
            std::optional<gfx::Point> origin);
  void Show(TabId glance);
  void Hide(BrowserWindowInterface* window,
            bool animate,
            base::OnceClosure done);
  // Stops tracking it; it stays a tab.
  void Unmark(TabId glance);
  void OnParentClosing(TabId parent);
  void OnGlanceClosing(TabId glance);
  void OnClosingAllWindows(bool closing);
  void RedirectToGlance(BrowserWindowInterface* window, TabId parent);
  std::optional<TabId> ActiveGlance(BrowserWindowInterface* window);

  const raw_ref<Tabs> tabs_;
  const raw_ref<Windows> windows_;
  const raw_ref<AppLifecycle> app_lifecycle_;
  const raw_ref<GlanceSurface> surface_;
  const raw_ref<Delegate> delegate_;
  std::map<TabId, Glance> glances_;
  std::map<TabId, TabId> by_parent_;
  std::map<TabId, Expectation> expected_;
  // Tabs opened by an Essential or a pinned tab before their address was
  // known, with the tab that opened them.
  std::map<TabId, TabId> unknown_opened_;
  std::map<BrowserWindowInterface*, Watched> watched_;
  // The glance each window shows.
  std::map<BrowserWindowInterface*, TabId> shown_;
  base::CallbackListSubscription closing_all_;
  base::RepeatingClosureList on_changed_;
  base::WeakPtrFactory<GlanceRegistry> weak_factory_{this};
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_UI_GLANCE_GLANCE_REGISTRY_H_
