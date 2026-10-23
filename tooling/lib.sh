#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Adapted from Stedding tooling/lib.sh @ 8a15306
# (https://github.com/ysalitrynskyi/stedding-browser), Copyright (c) 2026
# Yevhen Salitrynskyi, BSD-3-Clause: see third_party/stedding/LICENSE.
#
# Shared environment for Cromarc tooling. Source this; do not execute it.
#
#   $CROMARC_ROOT     this repository          (default: parent of tooling/)
#   $DEPOT_TOOLS_DIR  Chromium's depot_tools   (default: ~/depot_tools)
#   $CHROMIUM_ROOT    gclient checkout root    (default: ~/chromium)
#   $CHROMIUM_SRC     Chromium source tree     (default: $CHROMIUM_ROOT/src)
#   $GIT_CACHE_PATH   depot_tools git cache    (default: $CHROMIUM_ROOT/.git-cache)
#
# The Chromium tree lives outside this repository and is never committed here.

set -euo pipefail

CROMARC_ROOT="${CROMARC_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-$HOME/depot_tools}"
CHROMIUM_ROOT="${CHROMIUM_ROOT:-$HOME/chromium}"
CHROMIUM_SRC="${CHROMIUM_SRC:-$CHROMIUM_ROOT/src}"
GIT_CACHE_PATH="${GIT_CACHE_PATH:-$CHROMIUM_ROOT/.git-cache}"
export CROMARC_ROOT DEPOT_TOOLS_DIR CHROMIUM_ROOT CHROMIUM_SRC GIT_CACHE_PATH

# shellcheck source=CHROMIUM_VERSION
. "$CROMARC_ROOT/CHROMIUM_VERSION"
export CHROMIUM_TAG CHROMIUM_MILESTONE CHROMIUM_COMMIT CHROMIUM_CHANNEL CHROMIUM_PIN_DATE

CHROMIUM_URL="https://chromium.googlesource.com/chromium/src.git"
export CHROMIUM_URL

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*" >&2; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31mxx\033[0m %s\n' "$*" >&2; exit 1; }

# Call as `require_macos_arm64 "$@"` first thing in a script. A script started by an
# x86_64 shell (an Intel Homebrew bash in /usr/local/bin) runs under Rosetta, and then
# depot_tools fetches x86_64 tools and the whole build runs translated: re-run natively.
require_macos_arm64() {
  [ "$(uname -s)" = "Darwin" ] || die "macOS only (got $(uname -s))"
  if [ "$(sysctl -n sysctl.proc_translated 2>/dev/null || echo 0)" = 1 ]; then
    exec arch -arm64 /bin/bash "$0" "$@"
  fi
  [ "$(uname -m)" = "arm64" ] || die "arm64 only (got $(uname -m))"
}

# The checkout brings its own Go (Dawn's code generators run it); a GOROOT or GOFLAGS
# from the user's shell points it at another toolchain ('go: no such tool "compile"').
sanitize_build_env() {
  unset GOROOT GOFLAGS
}

# Since Xcode 26 the Metal compiler is a separate download; ANGLE compiles Metal shaders
# with it. `xcrun -f metal` finds a stub even when it is missing, so run it.
require_metal_toolchain() {
  "${XCRUN:-/usr/bin/xcrun}" metal --version >/dev/null 2>&1 || die "the Metal Toolchain is not installed (ANGLE needs it). Install it once:
       xcodebuild -downloadComponent MetalToolchain"
}

# gclient picks host tools by the arch it runs under. Started under Rosetta (gclient is
# `#!/usr/bin/env bash`, so an x86_64 bash first in PATH is enough) it fetches x86_64 ones:
# the whole build then runs translated and gn sets host_cpu = "x64".
CHROMIUM_HOST_TOOLS=(buildtools/mac/gn third_party/siso/cipd/siso
  "third_party/llvm-build/Release+Asserts/bin/clang" third_party/rust-toolchain/bin/rustc)

require_arm64_host_tools() {
  local src="$1" tool archs bad=""
  for tool in "${CHROMIUM_HOST_TOOLS[@]}"; do
    archs="$(lipo -archs "$src/$tool" 2>/dev/null)" || archs="missing"
    case " $archs " in
      *" arm64 "*) ;;
      *) bad="$bad
       $tool: $archs" ;;
    esac
  done
  [ -z "$bad" ] && return 0
  die "host tools in $src are not arm64:$bad
     gclient ran under Rosetta. From a native shell, re-run: tooling/sync
     depot_tools keeps its own CIPD tools regardless of arch; if they are x86_64 too
     (lipo -archs $DEPOT_TOOLS_DIR/.cipd_bin/luci-auth), first: rm -rf $DEPOT_TOOLS_DIR/.cipd_bin"
}

add_depot_tools_to_path() {
  [ -d "$DEPOT_TOOLS_DIR" ] || die "depot_tools not found at $DEPOT_TOOLS_DIR — run tooling/bootstrap"
  case ":$PATH:" in
    *":$DEPOT_TOOLS_DIR:"*) ;;
    *) PATH="$DEPOT_TOOLS_DIR:$PATH" ;;
  esac
  export PATH
}

git_cache_dir() {
  git cache exists --cache-dir "$GIT_CACHE_PATH" "$1" 2>/dev/null
}

free_gb() {
  df -Pk "$1" | awk 'NR==2 { printf "%d", $4 / 1024 / 1024 }'
}

require_free_gb() {
  local path="$1" need="$2" have
  mkdir -p "$path"
  have="$(free_gb "$path")"
  [ "$have" -ge "$need" ] || die "need ${need} GB free on the volume holding $path, have ${have} GB"
  log "disk: ${have} GB free on the volume holding $path (need ${need} GB)"
}

# Chromium's TypeScript build resolves modules by walking up parent directories, so a
# node_modules above the checkout leaks its @types into the build.
require_no_ancestor_node_modules() {
  local dir found=""
  dir="$(cd "$1" 2>/dev/null && pwd || echo "$1")"
  [ -e "$dir/node_modules" ] && found="$dir/node_modules"
  while [ "$dir" != "/" ] && [ -n "$dir" ]; do
    dir="$(dirname "$dir")"
    [ -e "$dir/node_modules" ] && found="$dir/node_modules"
  done
  [ -z "$found" ] && return 0
  die "found $found
     Chromium's TypeScript build walks up parent directories looking for node_modules,
     and this one breaks it ('Undeclared dependencies to definition files').
     Put the checkout where no ancestor has node_modules:
         CHROMIUM_ROOT=/Users/Shared/chromium tooling/sync"
}

# The browser bundle in an out/ directory, skipping helper apps.
find_app_bundle() {
  local candidate app=""
  for candidate in "$1"/*.app; do
    [ -d "$candidate" ] || continue
    case "$(basename "$candidate")" in
      *Helper*) continue ;;
    esac
    app="$candidate"
  done
  [ -n "$app" ] && printf '%s' "$app"
}
