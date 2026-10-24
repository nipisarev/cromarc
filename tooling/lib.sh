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

# Hook commits made in the Chromium tree end up, via patches/hooks, in this public
# repository: give that tree this repository's identity, never the global one (on this
# machine it is a work address).
use_cromarc_identity() {
  local name email
  email="$(git -C "$CROMARC_ROOT" config user.email)" || die "set user.email in $CROMARC_ROOT first"
  name="$(git -C "$CROMARC_ROOT" config user.name)" || die "set user.name in $CROMARC_ROOT first"
  git -C "$CHROMIUM_SRC" config user.email "$email"
  git -C "$CHROMIUM_SRC" config user.name "$name"
}

require_clean_chromium_tree() {
  local changes
  changes="$(git -C "$CHROMIUM_SRC" status --porcelain --untracked-files=no)"
  [ -z "$changes" ] || die "uncommitted changes in $CHROMIUM_SRC:
$changes
     Commit them on cromarc-work and run tooling/export-patches, or discard them."
}

# Refuses when cromarc-work has a commit on top of $1 that is not in
# patches/hooks: rebuilding or leaving the branch would lose it.
require_exported_patches() {
  local base="$1" exported id commit
  git -C "$CHROMIUM_SRC" rev-parse -q --verify refs/heads/cromarc-work >/dev/null || return 0
  shopt -s nullglob
  local files=("$CROMARC_ROOT"/patches/hooks/*.patch)
  shopt -u nullglob
  # ${a[@]+...}: /bin/bash 3.2 treats an empty array as unset under set -u.
  exported="$(cat /dev/null ${files[@]+"${files[@]}"} | git -C "$CHROMIUM_SRC" patch-id --stable | cut -d' ' -f1 | sort -u)"
  while read -r id commit; do
    [ -n "$id" ] || continue
    grep -qx "$id" <<<"$exported" || die "cromarc-work has $(git -C "$CHROMIUM_SRC" log -1 --format='%h "%s"' "$commit"), which is not in patches/hooks.
     Keep it: tooling/export-patches, then re-run. Drop it: git -C $CHROMIUM_SRC branch -D cromarc-work"
  done < <(git -C "$CHROMIUM_SRC" log -p --reverse "$base..refs/heads/cromarc-work" | git -C "$CHROMIUM_SRC" patch-id --stable)
}

# src/ is mirrored into the Chromium tree as //cromarc; git there ignores it.
mirror_cromarc_sources() {
  mkdir -p "$CHROMIUM_SRC/cromarc"
  rsync -a --delete "$CROMARC_ROOT/src/" "$CHROMIUM_SRC/cromarc/"
  grep -qx '/cromarc/' "$CHROMIUM_SRC/.git/info/exclude" 2>/dev/null ||
    echo '/cromarc/' >> "$CHROMIUM_SRC/.git/info/exclude"
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

# Local code signing. An ad-hoc signature's designated requirement is the
# build's cdhash, so the Keychain asks again for "Chromium Safe Storage" after
# every build. Signed with one self-signed certificate, the requirement is the
# bundle id and that certificate: the same for every build. The certificate
# lives in a keychain of its own, not the login one, and is trusted for
# nothing.
SIGNING_IDENTITY="${CROMARC_SIGNING_IDENTITY:-Cromarc Local Signing}"
SIGNING_KEYCHAIN="${CROMARC_SIGNING_KEYCHAIN:-$HOME/Library/Keychains/cromarc-signing.keychain-db}"

have_signing_identity() {
  [ -f "$SIGNING_KEYCHAIN" ] &&
    security find-certificate -c "$SIGNING_IDENTITY" "$SIGNING_KEYCHAIN" >/dev/null 2>&1
}

# The keychain has no password: one would have to sit in plain text next to it.
unlock_signing_keychain() {
  security unlock-keychain -p "" "$SIGNING_KEYCHAIN"
}

create_signing_identity() {
  have_signing_identity && return 0
  local tmp
  tmp="$(mktemp -d)"
  printf '%s\n' '[req]' 'distinguished_name = dn' 'x509_extensions = ext' 'prompt = no' \
    '[dn]' "CN = $SIGNING_IDENTITY" \
    '[ext]' 'basicConstraints = critical, CA:false' 'keyUsage = critical, digitalSignature' \
    'extendedKeyUsage = critical, codeSigning' > "$tmp/cert.cnf"
  # /usr/bin/openssl (LibreSSL): `security import` reads its PKCS#12, not
  # the AES-encrypted one OpenSSL 3 writes by default.
  /usr/bin/openssl req -x509 -newkey rsa:2048 -nodes -days 7300 -config "$tmp/cert.cnf" \
    -keyout "$tmp/key.pem" -out "$tmp/cert.pem" 2>/dev/null
  /usr/bin/openssl pkcs12 -export -inkey "$tmp/key.pem" -in "$tmp/cert.pem" \
    -name "$SIGNING_IDENTITY" -passout pass:cromarc -out "$tmp/identity.p12"
  [ -f "$SIGNING_KEYCHAIN" ] || security create-keychain -p "" "$SIGNING_KEYCHAIN"
  security set-keychain-settings "$SIGNING_KEYCHAIN"
  unlock_signing_keychain
  security import "$tmp/identity.p12" -k "$SIGNING_KEYCHAIN" -P cromarc -T /usr/bin/codesign >/dev/null
  security set-key-partition-list -S apple-tool:,apple: -s -k "" "$SIGNING_KEYCHAIN" >/dev/null
  rm -rf "$tmp"
  log "signing identity \"$SIGNING_IDENTITY\" in $SIGNING_KEYCHAIN"
}

sign_code() {
  codesign --force --timestamp=none --sign "$SIGNING_IDENTITY" --keychain "$SIGNING_KEYCHAIN" "$1" 2>&1 |
    { grep -v ': replacing existing signature$' || true; } >&2
}

# Signs `app` inside out: the framework's libraries and helpers, the
# framework, then the app. No hardened runtime, as the ad-hoc build had.
sign_app() {
  local app="$1" framework f
  unlock_signing_keychain
  for framework in "$app"/Contents/Frameworks/*.framework; do
    for f in "$framework"/Versions/Current/Libraries/*.dylib "$framework"/Versions/Current/Helpers/*; do
      [ -e "$f" ] && sign_code "$f"
    done
    sign_code "$framework"
  done
  sign_code "$app"
  codesign --verify --deep --strict "$app" || die "the signature of $app does not verify"
}
