# Hooks: Cromarc's changes inside Chromium

Every change Cromarc makes to Chromium's own files is a hook listed here.
`tooling/arch-check` (rule R3) refuses an unlisted patch. In Chromium's C++ a
hook adds `#include "cromarc/hooks/hooks.h"` and one call, one guard
(`if (cromarc::hooks::X(...)) return ...;`) or one wrapped value, never other
logic, and removes nothing. Build and branding hooks (`Kind: build`) edit
build files and strings as needed. See AGENTS.md, "Hooks".

The patches are exported from the `cromarc-work` branch of the Chromium
checkout (`tooling/export-patches`) and applied by `tooling/apply`.
`tooling/check-patches TAG` tells whether they apply to a newer Chromium.

When a hook stops applying after a bump: find the place again from its
contract (not from the old line numbers), put the same one-line call there,
re-export, and fix the adapter behind the call if Chromium's types changed.

### H0 build //cromarc into Chrome
Kind: build
Where: `chrome/browser/ui/BUILD.gn` (`ui` deps), `chrome/chrome_paks.gni`
(`chrome_extra_paks`), `third_party/lit/v3_0/BUILD.gn` (visibility),
`tools/gritsettings/resource_ids.spec`.
Contract: //cromarc links into the browser, its WebUI resources pak ships, Lit is
visible to //cromarc, Cromarc's grd has its resource ids.
If it moved: the targets that hold Chrome's browser UI and WebUI configs.

### H1 vertical tabs by default
Kind: code
Where: `chrome/browser/ui/tabs/tab_strip_prefs.cc`, `RegisterProfilePrefs`.
Calls: `cromarc::hooks::VerticalTabsByDefault(<Chrome's default>)`, wrapping the value.
Contract: a new profile shows vertical tabs (`prefs::kVerticalTabsEnabled` true).
If it moved: where `kVerticalTabsEnabled` is registered.

### H2 sidebar in the vertical tab strip region
Kind: code
Where: `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc`,
`VerticalTabStripRegionView::AddTabStripView`.
Calls: `cromarc::hooks::InstallSidebar(content_parent, <Chrome's AddChildViewAt(...)>, browser)`.
Contract: once per window, after Chrome adds its tab list to the region's content
area, Cromarc gets that parent and list: it hides the list and Chrome's bottom
container and separator, and puts its sidebar next to it.
If it moved: where the vertical tab strip's tab list view is added to the region.

### H3 a collapsed vertical tab strip has no width
Kind: code (the constant moved upstream in 157)
Where: `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h`,
`kCollapsedWidth`.
Calls: `cromarc::hooks::CollapsedVerticalTabStripWidth(<Chrome's width>)`, wrapping the value.
Contract: collapsed (Cromarc's compact mode), the region takes no width.
If it moved: the collapsed width of the vertical tab strip region.

### H4 register Cromarc WebUIs and their interfaces
Kind: code
Where: `chrome/browser/ui/webui/chrome_web_ui_configs.cc` (`RegisterChromeWebUIConfigs`),
`chrome/browser/chrome_browser_interface_binders_webui_parts_desktop.cc`
(`PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop`).
Calls: `cromarc::hooks::RegisterWebUIConfigs`, `cromarc::hooks::PopulateWebUIInterfaceBrokers`.
Contract: chrome://cromarc-* pages exist and can bind their Mojo interfaces.
If it moved: where Chrome registers its own WebUI configs and frame binders.

### H5 links from other apps go to Cromarc first
Kind: code
Where: `chrome/browser/app_controller_mac.mm`, `OpenStartupTabsInBrowserWithProfile`.
Calls: a guard on `cromarc::hooks::OpenExternalLinks(tabs, profile)`.
Contract: URLs macOS hands to the app reach Cromarc's routing (Little window,
space routes) before Chrome opens them; Cromarc says whether it took them.
If it moved: where AppController opens URLs from other apps.

### H6 Cmd+T, Cmd+L, Cmd+W and Cmd+O go through Cromarc
Kind: code
Where: `chrome/browser/ui/browser_command_controller.cc`,
`BrowserCommandController::HandleCommandWithDisposition`.
Calls: guards on `cromarc::hooks::ShowCommandBar`, `KeepWindowOnLastTabClose`,
`OpenLittleInSpace`, each before Chrome's own handling.
Contract: for IDC_NEW_TAB, IDC_FOCUS_LOCATION, IDC_CLOSE_TAB and IDC_OPEN_FILE
Cromarc may handle the command and say so (Cmd+W closes a glance shown or keeps
a space window on its last tab, Cmd+O expands a glance or moves a Little
window's page); otherwise Chrome's code runs.
If it moved: Chrome's command dispatch for those IDC_ commands.

### H7 Cromarc's name and app icon
Kind: build
Where: `chrome/BUILD.gn` (icns, Assets.car), `chrome/app/chromium_strings.grd`,
`chrome/app/theme/chromium/BRANDING`.
Contract: the app is called Cromarc and has its icon.
If it moved: Chromium branding files.

### H8 build Cromarc keyed service factories
Kind: code
Where: `chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.cc`.
Calls: `cromarc::hooks::EnsureKeyedServiceFactoriesBuilt()`.
Contract: Cromarc's keyed services exist before profiles are created.
If it moved: where Chrome builds its own keyed service factories.

### H9 metrics tag for the Cromarc archive database
Kind: build
Where: `tools/metrics/histograms/metadata/sql/histograms.xml`.
Contract: the archive database's SQL histogram tag is declared (Chromium's
sql::Database checks it in debug builds).
If it moved: the SQL histogram tags list.

### H10 the link under the cursor goes to Cromarc too
Kind: code
Where: `chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.cc`,
`BrowserWebContentsDelegate::UpdateTargetURL`.
Calls: `cromarc::hooks::OnTargetURL(source, url)`.
Contract: Cromarc sees the link under the cursor (Glance's Option+click, which
opens it as a glance of the tab).
If it moved: where the browser learns a page's hovered link.

### H11 the page is a card outside split view
Kind: code
Where: `chrome/browser/ui/views/frame/multi_contents_view.cc`,
`MultiContentsView::CalculateProposedLayout`.
Calls: `cromarc::hooks::PageShowsBackground(...)` wrapping Chrome's
`show_background`, and `available_space = cromarc::hooks::PageSpace(...)`.
Contract: outside split view Cromarc gives the page's margins; with margins
the page shows on a background, as a card. While the active tab is a glance,
the page area is the glance's card (7.5% in from each side) and shows on a
background.
If it moved: the layout of the web contents area, outside split view or while
a glance shows.

### H12 the collapse button shows the sidebar icon
Kind: code
Where: `chrome/browser/ui/tabs/vertical_tab_strip_state_controller_impl.cc`,
`UpdateCollapseActionItem`.
Calls: `cromarc::hooks::CollapseButtonIcon(<Chrome's icon>)`, wrapping the value.
Contract: the collapse action shows a sidebar icon in both states.
If it moved: where the vertical tab strip's collapse action gets its icon.

### H13 glass frame by default
Kind: code
Where: `ui/base/ui_base_features.cc`, `kGlassFrame`.
Calls: `cromarc::hooks::GlassFrameDefault(<Chrome's default>)`, a constexpr wrapping the value.
Contract: the glass window frame is on by default.
If it moved: the glass frame feature's definition.

### H14 the space theme is the window's background
Kind: code
Where: `chrome/browser/ui/views/frame/browser_frame_view_mac.mm` (`OnPaint`),
`browser_native_widget_mac.mm` (`UpdateGlassEligibility`),
`custom_corners_background.cc` (`Paint`), `multi_contents_background_view.cc`.
Calls: `cromarc::hooks::PaintWindowBackground`; guards on `ThemesWindow`,
`PaintChromeBackground`, `ClearSolidLayer` and `InstallWindowMaterial`.
Contract: in a space window Cromarc paints the theme under the whole window and
Chrome's frame, toolbar and page backgrounds let it show. Once Chrome has made
a space window non-opaque for its glass frame, Cromarc puts Zen's window
material under it instead of Chrome's glass, tint and opaque backing. While
a glance shows, Cromarc paints its tab's page, dimmed and scaled to 0.97, over
the theme and under the glance's card.
If it moved: the Mac browser frame's painting; where the glass frame creates
its background views.

### H15 space windows keep their glass
Kind: code
Where: `chrome/browser/ui/views/frame/glass_frame_service.cc`,
`GlassFrameService::ActivationOrderedEligibleBrowsers`.
Calls: `cromarc::hooks::WithSpaceWindows(<Chrome's eligible windows>)`,
wrapping the returned set; Chrome's own selection is untouched.
Contract: space windows keep the glass frame whether active or not, beyond
Chrome's limit of glass windows.
If it moved: where Chrome picks which windows get the glass frame.

### H16 the Library takes the sidebar column while it is out
Kind: code
Where: `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc`,
`VerticalTabStripRegionView::CalculatePreferredSize`.
Calls: `size = cromarc::hooks::RegionSize(browser, size)`.
Contract: Cromarc may widen the region (the Library); otherwise Chrome's width.
If it moved: the vertical tab strip region's preferred width.

### H17 the compact sidebar blurs what it floats over
Kind: code
Where: `chrome/browser/ui/views/frame/custom_corners_background.cc`,
`CustomCornersBackground::SetUseBackgroundBlur`.
Calls: a guard on `cromarc::hooks::SetChromeBackgroundBlur(view, browser_view, layer, path)`.
Contract: for the floating compact sidebar Cromarc sets the layer's backdrop
blur; otherwise Chrome's blur setting.
If it moved: where the region's background sets its blur.

### H18 the page is a card all round, under a clear bookmarks bar
Kind: code
Where: `chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.cc`
(`CalculateProposedLayout`), `multi_contents_view.cc` (top separator),
`themed_background.cc` (`Paint`), `chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.cc`.
Calls: `content_corners = cromarc::hooks::PageCorners(...)`,
`should_show = cromarc::hooks::ShowsTopSeparator(...)`, a guard on
`PaintsOnSpaceTheme`, `TakesRoom(combo_button_)` wrapping Chrome's two
conditions and `ComboButtonWidth(...)` wrapping its width.
Contract: the page's corners are all round when it is a card, no line over it,
the bookmarks bar lets the theme show, and a hidden tab-strip combo button takes
no room in the top row.
If it moved: the browser view's content corners, the multi contents top
separator, the themed background, the vertical tab strip's top container layout.

### H19 the sidebar takes the region's whole height
Kind: code
Where: `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc`,
`VerticalTabStripRegionView::CalculateProposedLayout`.
Calls: `button_size = cromarc::hooks::RegionTopRowSize(browser, <Chrome's top
row preferred size>)`, `.visible = cromarc::hooks::RegionTopRowVisible(browser,
true)` for the top row's layout and `vertical_padding =
cromarc::hooks::RegionBottomPadding(browser, <Chrome's padding constant>)`, each
wrapping Chrome's value.
Contract: in a space window the row of top buttons is hidden with no height:
the sidebar draws Zen's top row itself (single toolbar, see H24), running from
the window's top with only the window buttons over it. While the Library is out
there is also no padding under the content, so it reaches the bottom edge, as
Zen's Library does. Other windows (incognito) get Chrome's values.
If it moved: the vertical tab strip region's layout of its top button row and
the padding at its bottom.

### H20 Cromarc's strings ship in every locale pak
Kind: build
Where: `chrome/chrome_repack_locales.gni` (`chrome_repack_locales`, next to
Indigo's strings), `tools/gritsettings/resource_ids.spec`.
Contract: each locale pak Chrome builds (`<locale>.lproj/locale.pak`) holds
`//cromarc/strings`'s messages in that locale, English where there is no
translation, so Cromarc's text comes in the language Chromium picked for its
own; `cromarc_strings.grd` has its message ids.
If it moved: the template that repacks the per-locale string paks of
`//chrome`, and the grd id ranges.

### H21 the Dock reopens the last active space
Kind: code
Where: `chrome/browser/app_controller_mac.mm`,
`-applicationShouldHandleReopen:hasVisibleWindows:`, after the guest and
signed-out check, before Chrome's own restore or new window.
Calls: a guard on `cromarc::hooks::ReopenSpace(lastProfilePath)`.
Contract: with no browser window on screen, a click on the Dock icon brings
back the space of the last used profile in one window, with its tabs when its
window was closed, as in Zen; for a profile that is no space Chrome's code
runs.
If it moved: where Chrome handles the Dock icon's reopen with no window
visible, once it knows the last used profile.

### H22 Cromarc sees a tab's navigations before they start
Kind: code
Where: `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`,
`CreateAndAddChromeThrottlesForNavigation`, in the main frame's block after
the throttles that only observe.
Calls: `cromarc::hooks::AddNavigationThrottles(registry)`.
Contract: Cromarc may add its throttles for a tab's main frame navigations:
a link the user follows from an Essential's or a pinned tab's page to another
site is cancelled there and opens in a glance over the tab, as Zen does on
Firefox's app tabs; a tab a page opened learns its first address (window.open).
If it moved: where Chrome adds its own navigation throttles for a navigation.

### H23 Cromarc adds to the page's context menu
Kind: code
Where: `chrome/browser/renderer_context_menu/render_view_context_menu.cc`,
`RenderViewContextMenu::InitMenu`, at its end.
Calls: `cromarc::hooks::AddContextMenuItems(this, menu_model_, observers_)`.
Contract: Cromarc joins the menu as a `RenderViewContextMenuObserver`, adds
"Open Link in Glance" before the separator that ends the open-link items, and
sees "Search … for …" before it runs, so its result opens as a glance, as in
Zen; Chrome's items and their handling are untouched.
If it moved: the end of the context menu's item building, where the menu
model and its observer list are at hand. The hook takes `observers_` as
Chromium declares it (now a `base::ReentrantObserverList`); its type follows
that declaration.

### H24 a space window shows no toolbar above the page
Kind: code
Where: `chrome/browser/ui/views/frame/browser_view.cc`,
`BrowserView::IsToolbarVisible`, its final return.
Calls: `return cromarc::hooks::ToolbarVisible(browser_.get(), <Chrome's
value>);`, wrapping Chrome's value.
Contract: in a window whose sidebar holds the toolbar (a space window, not an
incognito one) the toolbar is not visible: the page runs to the window's top
and the sidebar holds the window buttons, back, forward, reload and the
address field, as Zen's single toolbar does; Chrome lays out, anchors bubbles
and handles keys as it does for a window without a toolbar. Elsewhere Chrome's
value.
If it moved: the BrowserView method layouts and bubbles ask for whether the
toolbar shows.

### H25 the sidebar's empty space moves the window
Kind: code
Where: `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc`,
`VerticalTabStripRegionView::IsPositionInWindowCaption`, before the loop that
absorbs clicks on the region's children.
Calls: a guard on `cromarc::hooks::IsSidebarCaption(browser, this, point)`.
Contract: a point on the part of Cromarc's sidebar page marked
`app-region: drag` (its empty space, as Zen's draggable sidebar) counts as the
window's caption, so a drag there moves the window; other points on the
sidebar stay clicks on it, and Chrome's own rules apply elsewhere.
If it moved: where the vertical tab strip region answers whether a point is
window caption.
