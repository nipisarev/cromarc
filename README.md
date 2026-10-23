# Cromarc

An Arc/Zen-style browser built on vanilla Chromium, where every space is a full Chromium profile.

- **Spaces are real profiles.** Each space has its own extensions, cookies, passwords and history, so a personal space can run one password manager and a work space another.
- **Sidebar-only tab UI.** Vertical tabs with nested folders, pinned tabs that reset to their home URL, Essentials, a command bar, Glance, compact mode.
- **Tab hygiene without losing context.** Idle tabs close on their own and go to a searchable archive.
- **Small upstream footprint.** Our code lives in new files; upstream Chromium is touched only by small hook patches, so security releases can be rebuilt quickly.

## Status

Early development. The tooling builds vanilla Chromium at the pinned version; no Cromarc features yet.

## Building (macOS, Apple Silicon)

Needs full Xcode and about 100 GB free disk. The Chromium tree lives outside this repository, in `~/chromium`.

```bash
tooling/bootstrap        # depot_tools in ~/depot_tools
tooling/sync             # Chromium at the tag in CHROMIUM_VERSION
tooling/apply            # src/ -> chromium/src/cromarc, patches/hooks -> branch cromarc-work
tooling/build release    # out/release/Chromium.app
tooling/check            # shellcheck, tooling tests, WebUI unit tests
```

Our code lives in `src/` and is mirrored into the Chromium tree as `//cromarc`. The few edits to upstream files are commits on `cromarc-work`, kept in `patches/hooks/`: to change one, commit on `cromarc-work` and run `tooling/export-patches`.

Issues and pull requests are closed for now.

## Credits

- [Chromium](https://www.chromium.org/) (BSD-3-Clause)
- [Zen Browser](https://github.com/zen-browser/desktop) (MPL-2.0): reference for spaces, sidebar and tab behavior
- [Brave](https://github.com/brave/brave-core) (MPL-2.0): approach to maintaining a Chromium fork
- [Stedding](https://github.com/ysalitrynskyi/stedding-browser) (BSD-3-Clause): build tooling the scripts in `tooling/` are adapted from ([license](third_party/stedding/LICENSE))

## License

[MPL-2.0](LICENSE)
