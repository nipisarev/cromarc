# Cromarc

An Arc/Zen-style browser built on vanilla Chromium, where every space is a full Chromium profile.

- **Spaces are real profiles.** Each space has its own extensions, cookies, passwords and history, so a personal space can run one password manager and a work space another.
- **Sidebar-only tab UI.** Vertical tabs with nested folders, pinned tabs that reset to their home URL, Essentials, a command bar, Glance, compact mode.
- **Tab hygiene without losing context.** Idle tabs close on their own and go to a searchable archive.
- **Small upstream footprint.** Our code lives in new files; upstream Chromium is touched only by small hook patches, so security releases can be rebuilt quickly.

## Status

Early development, used daily by its author. Working: the sidebar with nested folders, pinned tabs and Essentials, the command bar, idle tabs to an archive and the Library, spaces as profiles with Zen's switching animation, swipes and gradient themes, an Arc sidebar importer, Little Arc windows for links from other apps with Zen's link routing rules, Glance, compact mode, sync between Macs through iCloud Drive (one Mac at a time).

## Building (macOS, Apple Silicon)

Needs full Xcode and about 100 GB free disk. The Chromium tree lives outside this repository, in `~/chromium`.

```bash
tooling/bootstrap        # depot_tools in ~/depot_tools
tooling/sync             # Chromium at the tag in CHROMIUM_VERSION
tooling/apply            # src/ -> chromium/src/cromarc, patches/hooks -> branch cromarc-work
tooling/build release    # out/release/Cromarc.app
tooling/check            # shellcheck, tooling tests, WebUI unit tests
tooling/test-browser     # browser suites over CDP, debug checks fatal (--sites: popular sites)
```

Our code lives in `src/` and is mirrored into the Chromium tree as `//cromarc`. The few edits to upstream files are commits on `cromarc-work`, kept in `patches/hooks/`: to change one, commit on `cromarc-work` and run `tooling/export-patches`.

Keeping up with upstream:

```bash
tooling/check-patches 157.0.8103.21   # do the hooks apply to a tag? (no checkout needed)
tooling/bump 156.0.8078.70            # repin, sync, apply, build, test, log to docs/maintenance-log.md
tooling/zen-diff                      # what changed in the Zen code we ported (docs/zen-sources.txt)
```

Issues and pull requests are closed for now.

## Credits

- [Chromium](https://www.chromium.org/) (BSD-3-Clause)
- [Zen Browser](https://github.com/zen-browser/desktop) (MPL-2.0): reference for spaces, sidebar and tab behavior
- [Brave](https://github.com/brave/brave-core) (MPL-2.0): approach to maintaining a Chromium fork
- [Ionicons](https://ionic.io/ionicons) (MIT): space and sidebar icons, the set Zen's space picker uses ([license](third_party/ionicons/LICENSE))
- [Stedding](https://github.com/ysalitrynskyi/stedding-browser) (BSD-3-Clause): build tooling the scripts in `tooling/` are adapted from ([license](third_party/stedding/LICENSE))

## License

[MPL-2.0](LICENSE)
