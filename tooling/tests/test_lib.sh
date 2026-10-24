#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Unit tests for tooling/lib.sh. Run: tooling/tests/test_lib.sh
# shellcheck disable=SC2016  # the snippets passed to in_lib expand in the subshell

set -uo pipefail

TOOLING_DIR="$(cd "$(dirname "$0")/.." && pwd)"
failures=0
passes=0

# expect NAME DETAIL STATUS — STATUS is the exit code of the check just run.
expect() {
  local name="$1" detail="$2" status="$3"
  if [ "$status" -eq 0 ]; then
    passes=$((passes + 1))
    printf '  ok   %s\n' "$name"
  else
    failures=$((failures + 1))
    printf '  FAIL %s\n       %s\n' "$name" "$detail"
  fi
}

# Each test runs lib.sh in a fresh subshell so `die` (exit 1) cannot end the runner.
in_lib() {
  bash -c ". '$TOOLING_DIR/lib.sh'; $1" 2>&1
}

test_pin_is_loaded() {
  local out
  out="$(in_lib 'printf "%s|%s|%s" "$CHROMIUM_TAG" "$CHROMIUM_MILESTONE" "$CHROMIUM_COMMIT"')"
  [[ "$out" =~ ^[0-9]+\.0\.[0-9]+\.[0-9]+\|[0-9]+\|[0-9a-f]{40}$ ]]
  expect "pin: CHROMIUM_TAG, CHROMIUM_MILESTONE, CHROMIUM_COMMIT come from CHROMIUM_VERSION" "got: $out" $?
}

test_tag_matches_milestone() {
  local out
  out="$(in_lib 'echo "${CHROMIUM_TAG%%.*} $CHROMIUM_MILESTONE"')"
  [ "${out% *}" = "${out#* }" ]
  expect "pin: milestone is the tag's major" "got: $out" $?
}

test_default_paths() {
  local out want="/tmp/h/depot_tools|/tmp/h/chromium|/tmp/h/chromium/src|/tmp/h/chromium/.git-cache"
  out="$(env -u CHROMIUM_ROOT -u CHROMIUM_SRC -u GIT_CACHE_PATH -u DEPOT_TOOLS_DIR HOME=/tmp/h \
    bash -c ". '$TOOLING_DIR/lib.sh'; printf '%s|%s|%s|%s' \"\$DEPOT_TOOLS_DIR\" \"\$CHROMIUM_ROOT\" \"\$CHROMIUM_SRC\" \"\$GIT_CACHE_PATH\"" 2>&1)"
  [ "$out" = "$want" ]
  expect "paths: defaults under \$HOME" "got: $out" $?
}

test_free_gb_is_integer() {
  local out
  out="$(in_lib 'free_gb /')"
  [[ "$out" =~ ^[0-9]+$ ]]
  expect "free_gb: whole GB" "got: $out" $?
}

test_require_free_gb_dies_when_short() {
  local out status
  out="$(in_lib 'require_free_gb /tmp 999999')"; status=$?
  [[ "$status" -eq 1 && "$out" == *"need 999999 GB free"* ]]
  expect "require_free_gb: refuses with the numbers" "status=$status out=$out" $?
}

test_require_free_gb_passes_when_enough() {
  local status
  in_lib 'require_free_gb /tmp 0' >/dev/null; status=$?
  [ "$status" -eq 0 ]
  expect "require_free_gb: passes when there is room" "status=$status" $?
}

test_ancestor_node_modules_detected() {
  local tmp out status
  tmp="$(cd "$(mktemp -d)" && pwd -P)"
  mkdir -p "$tmp/node_modules" "$tmp/a/b"
  out="$(in_lib "require_no_ancestor_node_modules '$tmp/a/b'")"; status=$?
  rm -rf "$tmp"
  [[ "$status" -eq 1 && "$out" == *"found $tmp/node_modules"* ]]
  expect "node_modules: an ancestor's is refused" "status=$status out=$out" $?
}

test_no_ancestor_node_modules_passes() {
  local tmp status
  tmp="$(mktemp -d)"
  mkdir -p "$tmp/a/b"
  in_lib "require_no_ancestor_node_modules '$tmp/a/b'" >/dev/null; status=$?
  rm -rf "$tmp"
  [ "$status" -eq 0 ]
  expect "node_modules: clean ancestry passes" "status=$status" $?
}

test_missing_depot_tools_dies() {
  local out status
  out="$(DEPOT_TOOLS_DIR=/nonexistent/depot_tools bash -c ". '$TOOLING_DIR/lib.sh'; add_depot_tools_to_path" 2>&1)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"tooling/bootstrap"* ]]
  expect "depot_tools: missing directory names the remedy" "status=$status out=$out" $?
}

test_find_app_bundle_skips_helpers() {
  local tmp out
  tmp="$(mktemp -d)"
  mkdir -p "$tmp/Chromium.app" "$tmp/Chromium Helper.app"
  out="$(in_lib "find_app_bundle '$tmp'")"
  rm -rf "$tmp"
  [ "$out" = "$tmp/Chromium.app" ]
  expect "find_app_bundle: the browser, not a helper" "got: $out" $?
}

test_reexecs_native_under_rosetta() {
  local tmp out
  if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    echo "  skip  arch: Rosetta is not installed"
    return 0
  fi
  tmp="$(mktemp -d)"
  printf '. "%s/lib.sh"\nrequire_macos_arm64 "$@"\nprintf "%%s %%s" "$(uname -m)" "$*"\n' "$TOOLING_DIR" > "$tmp/probe.sh"
  out="$(arch -x86_64 /bin/bash "$tmp/probe.sh" one two 2>&1)"
  rm -rf "$tmp"
  [ "$out" = "arm64 one two" ]
  expect "arch: a script started under Rosetta re-runs natively with its arguments" "got: $out" $?
}

fake_xcrun() {
  # $1: exit status of `xcrun metal --version`
  local dir
  dir="$(mktemp -d)"
  printf '#!/bin/bash\necho "fake metal" >&2\nexit %s\n' "$1" > "$dir/xcrun"
  chmod +x "$dir/xcrun"
  echo "$dir/xcrun"
}

test_metal_toolchain_missing_dies() {
  local xcrun out status
  xcrun="$(fake_xcrun 1)"
  out="$(XCRUN="$xcrun" bash -c ". '$TOOLING_DIR/lib.sh'; require_metal_toolchain" 2>&1)"; status=$?
  rm -rf "$(dirname "$xcrun")"
  [[ "$status" -eq 1 && "$out" == *"xcodebuild -downloadComponent MetalToolchain"* ]]
  expect "metal: a missing Metal Toolchain names the remedy" "status=$status out=$out" $?
}

test_metal_toolchain_present_passes() {
  local xcrun status
  xcrun="$(fake_xcrun 0)"
  XCRUN="$xcrun" bash -c ". '$TOOLING_DIR/lib.sh'; require_metal_toolchain" >/dev/null 2>&1; status=$?
  rm -rf "$(dirname "$xcrun")"
  [ "$status" -eq 0 ]
  expect "metal: an installed Metal Toolchain passes" "status=$status" $?
}

test_build_env_drops_toolchain_overrides() {
  local out
  out="$(GOROOT=/somewhere/go GOFLAGS=-mod=vendor bash -c ". '$TOOLING_DIR/lib.sh'; sanitize_build_env; printf '[%s][%s]' \"\${GOROOT-unset}\" \"\${GOFLAGS-unset}\"" 2>&1)"
  [ "$out" = "[unset][unset]" ]
  expect "build env: GOROOT and GOFLAGS from the user's shell do not reach the build" "got: $out" $?
}

# fake_src ARCH_OF_CLANG — a tree with the host tools gclient fetches; clang gets the given arch.
fake_src() {
  local src tool arch
  src="$(mktemp -d)"
  for tool in buildtools/mac/gn third_party/siso/cipd/siso \
      "third_party/llvm-build/Release+Asserts/bin/clang" third_party/rust-toolchain/bin/rustc; do
    arch=arm64
    case "$tool" in */clang) arch="$1" ;; esac
    mkdir -p "$src/$(dirname "$tool")"
    printf 'int main(void){return 0;}\n' | /usr/bin/cc -arch "$arch" -x c - -o "$src/$tool"
  done
  echo "$src"
}

test_arm64_host_tools_pass() {
  local src status
  src="$(fake_src arm64)"
  in_lib "require_arm64_host_tools '$src'" >/dev/null; status=$?
  rm -rf "$src"
  [ "$status" -eq 0 ]
  expect "host tools: an all-arm64 checkout passes" "status=$status" $?
}

test_x86_host_tool_dies() {
  local src out status
  src="$(fake_src x86_64)"
  out="$(in_lib "require_arm64_host_tools '$src'")"; status=$?
  rm -rf "$src"
  [[ "$status" -eq 1 && "$out" == *"bin/clang: x86_64"* && "$out" != *"gn:"* && "$out" == *"tooling/sync"* ]]
  expect "host tools: an x86_64 one is named with the remedy" "status=$status out=$out" $?
}

test_missing_host_tool_dies() {
  local src out status
  src="$(fake_src arm64)"
  rm "$src/third_party/siso/cipd/siso"
  out="$(in_lib "require_arm64_host_tools '$src'")"; status=$?
  rm -rf "$src"
  [[ "$status" -eq 1 && "$out" == *"siso: missing"* ]]
  expect "host tools: a missing one is named" "status=$status out=$out" $?
}

echo "tooling/lib.sh"
test_arm64_host_tools_pass
test_x86_host_tool_dies
test_missing_host_tool_dies
test_build_env_drops_toolchain_overrides
test_metal_toolchain_missing_dies
test_metal_toolchain_present_passes
test_reexecs_native_under_rosetta
test_pin_is_loaded
test_tag_matches_milestone
test_default_paths
test_free_gb_is_integer
test_require_free_gb_dies_when_short
test_require_free_gb_passes_when_enough
test_ancestor_node_modules_detected
test_no_ancestor_node_modules_passes
test_missing_depot_tools_dies
test_find_app_bundle_skips_helpers

printf '\n%d passed, %d failed\n' "$passes" "$failures"
[ "$failures" -eq 0 ]
