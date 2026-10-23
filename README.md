# Cromarc

An Arc/Zen-style browser built on vanilla Chromium, where every space is a full Chromium profile.

- **Spaces are real profiles.** Each space has its own extensions, cookies, passwords and history, so a personal space can run one password manager and a work space another.
- **Sidebar-only tab UI.** Vertical tabs with nested folders, pinned tabs that reset to their home URL, Essentials, a command bar, Glance, compact mode.
- **Tab hygiene without losing context.** Idle tabs close on their own and go to a searchable archive.
- **Small upstream footprint.** Our code lives in new files; upstream Chromium is touched only by small hook patches, so security releases can be rebuilt quickly.

## Status

Design phase. There is no code or build yet.

Issues and pull requests are closed for now.

## Credits

- [Chromium](https://www.chromium.org/) (BSD-3-Clause)
- [Zen Browser](https://github.com/zen-browser/desktop) (MPL-2.0): reference for spaces, sidebar and tab behavior
- [Brave](https://github.com/brave/brave-core) (MPL-2.0): approach to maintaining a Chromium fork

## License

[MPL-2.0](LICENSE)
