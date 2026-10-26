# AGENTS.md: how Cromarc is built, for agents and people

Cromarc is Chromium with Zen's and Arc's sidebar, spaces and pinned tabs. Our
code lives in `src/` (built into Chromium as `//cromarc`). Chromium's own files
change only through the hooks in `patches/hooks/`. Chromium ships a major
version every four weeks, and security fixes come only in the current one, so
Cromarc moves to every major. These rules exist so that such a move means
moving one-line hooks and fixing adapters, not untangling our features from
Chromium's internals.

## The rules in short

1. Feature code talks to Chromium only through **ports** (`src/browser/ports/`),
   interfaces of ours. Their **adapters** (`src/browser/chromium/`) are the only
   code that includes Chromium's internals.
2. A **hook** in a Chromium file is one call into `cromarc::hooks::`. Nothing
   else: no conditions, no loops, no edits to Chromium's control flow. Every
   hook is registered with its contract in `patches/hooks/README.md`.
3. Never find Chromium's views by class (`IsViewClass`/`AsViewClass`) or reach
   into `BrowserView` outside an adapter.
4. **System changes** (feature flags, Chrome's prefs, AppKit-wide tricks,
   default browser, keychain, files outside the profile, network) live only in
   the files listed in `docs/system-changes.md`, and their commits carry
   `System-Change: <why>`.
5. Never commit with a red check, never install a build that did not pass
   every suite, never add to `tooling/arch/baseline.txt`.
6. UI behaviour and look are ported from Zen's code
   (`~/Projects/_ref/zen-desktop/src/zen`), never invented. Where our model
   differs from Zen's, what the user sees, transitions included, still matches
   Zen.

`tooling/arch-check` enforces 1–4 (rules R1–R4 below). It runs when an agent
writes a file, at every commit, in CI and before an install.

## Layers

| Layer | Where | May include | Role |
|---|---|---|---|
| Hooks | `src/hooks/` (entry points), one-line calls in Chromium via `patches/hooks/` | anything | Chromium calls into Cromarc here and nowhere else |
| Adapters | `src/browser/chromium/`, WebUI controllers (`*_ui.{h,cc}`, `webui_registration`), keyed service factories (`*_factory.{h,cc}`) | Chromium's internals: `chrome/browser/**`, `chrome/common/pref_names.h`, `components/{tabs,browser_apis,bookmarks,favicon,history,download,sessions,omnibox,sync}/**` | implement ports on top of Chromium; compose services |
| Ports | `src/browser/ports/` | stable APIs only | narrow interfaces, one concern each; in their signatures our types, stable ones, our mojom types, and windows and profiles as handles only (`BrowserWindowInterface*`, `Profile*`, never dereferenced outside adapters) |
| Features | everything else in `src/browser/` | stable APIs: `base/`, `build/`, `url/`, `mojo/public/`, `net/base/`, `ui/{gfx,base,views,compositor,events,color,menus,display,accessibility,native_theme,shell_dialogs}/`, `cc/paint/`, `content/public/`, `components/{prefs,pref_registry,keyed_service}/`, `third_party/{skia,abseil-cpp,blink/public,re2}/`, `sql/`, `skia/ext/`, `net/traffic_annotation/`, `components/input/native_web_keyboard_event.h`, icon sets (`components/vector_icons/`, `chrome/app/vector_icons/`), our generated `chrome/grit/cromarc_*`, `testing/`, `cromarc/` | spaces, tree, lifecycle, sidebar, compact mode, Glance, Little, command bar, Library, routing |
| WebUI | `src/webui/` (TypeScript, Lit) | the Mojo interfaces in `src/browser/**/*.mojom` | the sidebar and pages; no Chromium C++ here |

Features get their ports through their constructor (dependency inversion).
The composition roots build the adapters and hand them in: keyed service
factories, WebUI controllers, `InstallSidebar()` (hook H2, for a window's
sidebar), `src/hooks/hooks.cc` (for the hook entry points), and the
`*_get.cc` files in `src/browser/chromium/` for the browser-wide singletons
(`SpaceController::Get()`, `SpaceRouting::Get()`). The process-wide adapters
are in `src/browser/chromium/adapters.h`.

Unit tests hand in fakes (`src/browser/ports/fake_*.{h,cc}`). A fake keeps
its port's contract; where a port has behaviour of its own, its contract
tests run on the adapter and on the fake alike (`Tree`:
`src/browser/chromium/bookmark_tree_unittest.cc`).

## Adding a feature

1. Write the behaviour against a port. If none fits, add a method to the
   narrowest port, or a new port with one concern. Types in the port are ours
   or stable ones (`GURL`, `gfx::Rect`, `base::` types), never Chromium's
   classes.
2. Implement the method in the port's adapter, in `src/browser/chromium/`.
3. Test the feature in `cromarc_unittests` with a fake port, and end to end
   with a browser suite in `tests/browser/suites/` (`tooling/test-browser`).
4. Only when Chromium has no extension point at all (observer, keyed service,
   WebUI config, pref, command) add a hook: see below.

## Hooks

- Form, in Chromium's file: `#include "cromarc/hooks/hooks.h"` and either
  `cromarc::hooks::DoSomething(args);`, a guard at the top of a block
  `if (cromarc::hooks::Handles(args)) return ...;`, or one wrapped value
  `x = cromarc::hooks::Adjust(args, <Chromium's value>);`. Comments may say why.
- Register it in `patches/hooks/README.md` as `### H<n> title` with Kind, Where,
  Calls, Contract and "If it moved". `Kind: build` marks build and branding
  hooks, which edit build files and strings as they need.
- Make the change on the `cromarc-work` branch of `~/chromium/src`, commit it as
  "Cromarc H<n>: …" with this repository's git identity, and run
  `tooling/export-patches`.
- Its commit here needs `System-Change:`: every change in `patches/hooks/` is one.

## System changes

Categories and the files allowed each are in `docs/system-changes.md`. To add
one, add the row (file, categories, why) and say `System-Change: <why>` in the
commit. Reading a command-line switch is not a system change; adding one is.

## Checks: how a violation is stopped

| When | What |
|---|---|
| An agent writes a file | `.claude/settings.json` runs `tooling/arch-check --claude-hook`; a violation comes back to the agent at once |
| `git commit` | `tooling/git-hooks/pre-commit` (arch-check on the staged files, the baseline only shrinks; all of `tooling/check` when tooling or WebUI files are staged) and `commit-msg` (`System-Change:`). `tooling/bootstrap` sets `core.hooksPath` |
| Push | CI (`.github/workflows/check.yml`): `tooling/check` (arch-check `--strict` and its tests), baseline growth, `System-Change:` on every pushed commit |
| Install | `tooling/install` refuses unless the tree is committed, arch-check passes, every suite passed on this very build (`tooling/test-browser` stamps it) and Chromium holds exactly `patches/hooks` |

`tooling/arch/baseline.txt` listed the violations that were there when the
check came in (350); it is empty now and stays empty. Nothing may be added
to it: a new need for Chromium in feature code is a port method and its
adapter, never an exception.

## Working order

- Build: `tooling/build release chrome cromarc_unittests` (it mirrors `src/`
  into Chromium). Never rebuild `out/release` while the daily Cromarc runs from
  it; the daily app is `/Applications/Cromarc.app`.
- Test: `tooling/test-browser` (every suite, headless) and
  `out/release/cromarc_unittests`, `node --test src/webui/*/tests/*_test.ts`,
  `tooling/check`. A visual change also needs a window screenshot
  (windowed run plus `screencapture -l <window id>`).
- Commit only after reading the results. Install: `tooling/install`.

## Updating Chromium

1. Forecast without a checkout: `tooling/check-patches <tag>` (do the hooks
   apply?) and `tooling/check-headers <tag>` (are the headers our code includes
   still there?). CI runs both daily against Stable, Beta and Dev.
2. `tooling/bump <tag>`. When a hook does not apply, the bump stops after the
   sync with `git am` at that hook in `~/chromium/src`: find the place again
   from its contract in `patches/hooks/README.md`, put the same one-line call
   there, `git add`, `git am --continue`. After the last one,
   `tooling/bump --continue` exports the moved hooks and goes on.
3. Build. Compile errors from Chromium's API changes are fixed in the adapters
   (`src/browser/chromium/`); feature code should not need a change. If it
   does, the port leaked, so fix the port.
4. Every suite, then record the bump in `docs/maintenance-log.md`.
