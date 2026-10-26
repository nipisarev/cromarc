# System changes

Changes that reach beyond Cromarc's own windows and data: Chromium-wide
switches and feature defaults, Chrome's own prefs, AppKit-wide tricks, the
default browser, the keychain, files outside the profile, the network.

Each one lives only in a file listed here with its category, and every commit
that touches one of these files, or the files below, carries a line
`System-Change: <why>` (tooling/git-hooks/commit-msg checks it).
`tooling/arch-check` (rule R4) refuses a system API anywhere else. See AGENTS.md,
"System changes".

Always system changes, whatever the diff: `patches/hooks/` (Chromium's own
files), `tooling/args/`, `DEPS`, `CHROMIUM_VERSION`, `branding/`,
`.github/workflows/`, this file, `tooling/arch/`, `tooling/arch-check`,
`tooling/git-hooks/`, `.claude/settings.json`.

Categories: `feature-flags` (base::FeatureList, AppendSwitch),
`chrome-prefs` (Chrome's and sync's prefs:: keys), `appkit-global` (NSEvent
monitors, swizzling, window alpha and mouse pass-through), `user-defaults`,
`default-browser`, `keychain`, `outside-profile` (home and iCloud paths),
`network`.

| File | Categories | Why |
|---|---|---|
| `src/browser/chromium/strip_defaults_adapter.cc` | chrome-prefs | Seeds `kVerticalTabsCollapsedState` and `kVerticalTabsUncollapsedWidth` in a space's profile before its window opens (SpaceController, through StripDefaults): one sidebar state for every space's window, as Zen's one window. |
| `src/browser/lifecycle/lifecycle_service_factory.cc` | chrome-prefs | `kRestoreOnStartup` defaults to the last session: tabs come back after a restart (design, lifecycle). |
| `src/browser/sync/icloud_sync.cc` | chrome-prefs outside-profile | Chromium's local sync backend in iCloud Drive (decision D12): `syncer::prefs::kEnableLocalSyncBackend`, `kLocalSyncBackendDir` = `~/Library/Mobile Documents/com~apple~CloudDocs/Cromarc`. Opt-in per space. |
| `src/browser/spaces/space_window_mac.mm` | appkit-global | A space window prepared under the visible one at 1% opacity, mouse passing through, so the swap at the end of a switch shows it painted. |
| `src/browser/ui/sidebar/space_input_mac.mm` | appkit-global | NSEvent local monitors (keys, scroll, mouse moves) for switching spaces; a global mouse monitor only while the compact sidebar is out (Zen's mouse tracker). |
| `src/browser/ui/welcome/welcome_page_handler.cc` | default-browser | The welcome page's button that asks macOS to make Cromarc the default browser. |
| `src/browser/ui/sidebar/library_media.cc` | outside-profile | The Library's media section reads the user's folders, after the user opts in. |
