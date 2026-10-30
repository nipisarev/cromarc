// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_CHROMIUM_GLANCE_MENU_OBSERVER_H_
#define CROMARC_BROWSER_CHROMIUM_GLANCE_MENU_OBSERVER_H_

#include "base/observer_list.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_user_data.h"

class RenderViewContextMenu;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace cromarc {

// Glance in a page's context menu (hook H23), as Zen has it: "Open Link in
// Glance" after the open-link items, and "Search … for …" opening its result
// as a glance of the tab. One per page, joining each menu it opens.
class GlanceMenuObserver
    : public RenderViewContextMenuObserver,
      public content::WebContentsUserData<GlanceMenuObserver> {
 public:
  static void Attach(
      RenderViewContextMenu* menu,
      ui::SimpleMenuModel& model,
      base::ReentrantObserverList<RenderViewContextMenuObserver>::
          UncheckedAndDanglingUntriaged& observers);

  GlanceMenuObserver(const GlanceMenuObserver&) = delete;
  GlanceMenuObserver& operator=(const GlanceMenuObserver&) = delete;
  ~GlanceMenuObserver() override;

  // RenderViewContextMenuObserver:
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;
  void CommandWillBeExecuted(int command_id) override;

 private:
  friend WebContentsUserData;

  explicit GlanceMenuObserver(content::WebContents* contents);

  // Where the menu opened, on the screen: the glance grows out of it.
  gfx::Point MenuPoint();

  content::ContextMenuParams params_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_CHROMIUM_GLANCE_MENU_OBSERVER_H_
