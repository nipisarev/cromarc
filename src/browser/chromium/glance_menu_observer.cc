// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/glance_menu_observer.h"

#include <optional>
#include <vector>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/chromium/adapters.h"
#include "cromarc/browser/ports/tabs.h"
#include "cromarc/browser/ports/windows.h"
#include "cromarc/browser/ui/glance/glance_menu.h"
#include "cromarc/browser/ui/glance/glance_registry.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace cromarc {

namespace {

// Free in chrome_command_ids.h (below IDC_CONTENT_CONTEXT_CUSTOM_FIRST).
constexpr int kOpenLinkInGlance = 46900;

MenuContext ContextOf(content::WebContents* contents,
                      const content::ContextMenuParams& params) {
  MenuContext context;
  context.link = params.link_url;
  const std::optional<TabId> tab = ChromiumTabs().FromContents(contents);
  BrowserWindowInterface* window =
      tab ? ChromiumTabs().GetWindow(*tab) : nullptr;
  context.in_normal_tab = window && ChromiumWindows().IsNormal(window) &&
                          !ChromiumWindows().IsOffTheRecord(window);
  context.in_glance = tab && GlanceRegistry::Get()->IsGlance(*tab);
  return context;
}

}  // namespace

// static
void GlanceMenuObserver::Attach(
    RenderViewContextMenu* menu,
    ui::SimpleMenuModel& model,
    base::ReentrantObserverList<RenderViewContextMenuObserver>::
        UncheckedAndDanglingUntriaged& observers) {
  content::WebContents* contents = menu->GetWebContents();
  if (!contents) {
    return;
  }
  const MenuContext context = ContextOf(contents, menu->params());
  const bool link = OffersOpenLinkInGlance(context);
  if (!link && !SearchOpensInGlance(context)) {
    return;
  }
  CreateForWebContents(contents);
  GlanceMenuObserver* observer = FromWebContents(contents);
  observer->params_ = menu->params();
  observers.AddObserver(observer);
  if (!link) {
    return;
  }
  std::vector<int> commands;
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    commands.push_back(model.GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR
                           ? kMenuSeparator
                           : model.GetCommandIdAt(i));
  }
  std::optional<size_t> index =
      OpenLinkInGlanceIndex(commands, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  if (!index) {
    index = OpenLinkInGlanceIndex(commands, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  }
  if (index) {
    model.InsertItemAt(*index, kOpenLinkInGlance,
                       l10n_util::GetStringUTF16(IDS_CROMARC_GLANCE_OPEN_LINK));
  }
}

GlanceMenuObserver::GlanceMenuObserver(content::WebContents* contents)
    : WebContentsUserData(*contents) {}

GlanceMenuObserver::~GlanceMenuObserver() = default;

bool GlanceMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == kOpenLinkInGlance;
}

bool GlanceMenuObserver::IsCommandIdEnabled(int command_id) {
  return true;
}

void GlanceMenuObserver::ExecuteCommand(int command_id) {
  const std::optional<TabId> tab =
      ChromiumTabs().FromContents(&GetWebContents());
  if (command_id != kOpenLinkInGlance || !tab ||
      !GlanceRegistry::Get()->Expect(*tab, MenuPoint())) {
    return;
  }
  // As Chrome's "Open Link in New Tab" opens it, but as a glance.
  content::OpenURLParams open(
      params_.link_url,
      content::Referrer(params_.frame_url.is_empty() ? params_.page_url
                                                      : params_.frame_url,
                        params_.referrer_policy),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  open.initiator_origin = params_.frame_origin;
  ChromiumTabs().OpenFrom(*tab, open);
}

void GlanceMenuObserver::CommandWillBeExecuted(int command_id) {
  if (command_id != IDC_CONTENT_CONTEXT_SEARCHWEBFOR) {
    return;
  }
  // Chrome opens the result from this tab: in front, it is its glance (Zen's
  // onSearchSelectCommand); in the background, an ordinary tab.
  if (const std::optional<TabId> tab =
          ChromiumTabs().FromContents(&GetWebContents())) {
    GlanceRegistry::Get()->Expect(*tab, MenuPoint());
  }
}

gfx::Point GlanceMenuObserver::MenuPoint() {
  gfx::Point point(params_.x, params_.y);
  if (content::RenderWidgetHostView* view =
          GetWebContents().GetRenderWidgetHostView()) {
    point += view->GetViewBounds().OffsetFromOrigin();
  }
  return point;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlanceMenuObserver);

}  // namespace cromarc
