#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Unit tests for tooling/arch-check: each rule stops its kind of code, lets the
# right code through, and the baseline only shrinks. Run: tooling/tests/test_arch_check.sh
# shellcheck disable=SC2016  # registry rows hold literal backticks
# shellcheck disable=SC2319  # expect gets the status of the condition just tested

set -uo pipefail

# Run from a git hook, git's own variables point at this repository and its
# commit (GIT_INDEX_FILE, GIT_AUTHOR_EMAIL...); the throwaway trees below must
# not see them.
while read -r var; do unset "$var"; done < <(git rev-parse --local-env-vars)
unset GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_AUTHOR_DATE \
  GIT_COMMITTER_NAME GIT_COMMITTER_EMAIL GIT_COMMITTER_DATE

TOOLING_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CHECK="$TOOLING_DIR/arch-check"
failures=0
passes=0
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

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

# A fresh repository with one clean feature and empty registries.
fixture() {
  local root="$work/$1"
  rm -rf "$root"
  mkdir -p "$root/src/browser/feature" "$root/src/browser/chromium" "$root/patches/hooks" \
    "$root/docs" "$root/tooling/arch"
  cat >"$root/src/browser/feature/clean.cc" <<'EOF'
#include "base/functional/bind.h"
#include "cromarc/browser/ports/tabs.h"
class OurView : public views::View {};
void F(views::View* v) { views::AsViewClass<OurView>(v); }
EOF
  printf '| File | Categories | Why |\n|---|---|---|\n' >"$root/docs/system-changes.md"
  printf '# Hooks\n' >"$root/patches/hooks/README.md"
  echo "$root"
}

# check ROOT ARGS... — prints output, returns arch-check's status.
check() {
  local root="$1"
  shift
  "$CHECK" --root "$root" "$@" 2>&1
}

hook_patch() {
  # hook_patch FILE ID BODY — a patch adding BODY to a Chromium .cc file.
  local file="$1" id="$2" body="$3"
  {
    printf 'Subject: [PATCH] Cromarc %s: test\n\n' "$id"
    printf 'diff --git a/chrome/browser/x.cc b/chrome/browser/x.cc\n'
    printf -- '--- a/chrome/browser/x.cc\n+++ b/chrome/browser/x.cc\n@@ -1,2 +1,4 @@ void X() {\n'
    printf '%s\n' "$body"
  } >"$file"
}

test_clean_passes() {
  local root out
  root="$(fixture clean)"
  out="$(check "$root")"
  expect "a clean repository passes" "$out" $?
}

test_r1_feature_include() {
  local root out status
  root="$(fixture r1)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/feature/bad.cc"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R1 src/browser/feature/bad.cc chrome/browser/ui/browser.h"* ]]
  expect "R1: a feature including chrome/browser/ui fails" "$status: $out" $?
}

test_r1_own_resources_allowed() {
  local root out status
  root="$(fixture r1res)"
  echo '#include "chrome/grit/cromarc_resources.h"' >"$root/src/browser/feature/ok.cc"
  out="$(check "$root")"
  status=$?
  echo '#include "chrome/grit/generated_resources.h"' >"$root/src/browser/feature/bad.cc"
  out="$status $(check "$root")"
  [[ "$out" == "0 "* ]] && [[ "$out" == *"R1 src/browser/feature/bad.cc chrome/grit/generated_resources.h"* ]]
  expect "R1: our generated chrome/grit/cromarc_* passes, Chrome's grit fails" "$out" $?
}

test_r1_foundation_allowed() {
  local root out status
  root="$(fixture r1lib)"
  printf '#include "sql/database.h"\n#include "third_party/re2/src/re2/re2.h"\n#include "components/vector_icons/vector_icons.h"\n' \
    >"$root/src/browser/feature/ok.cc"
  out="$(check "$root")"
  status=$?
  echo '#include "components/omnibox/browser/autocomplete_match.h"' >"$root/src/browser/feature/bad.cc"
  out="$status $(check "$root")"
  [[ "$out" == "0 "* ]] && [[ "$out" == *"R1 src/browser/feature/bad.cc components/omnibox/browser/autocomplete_match.h"* ]]
  expect "R1: foundation libraries and icon sets pass, other components fail" "$out" $?
}

test_r1_adapter_allowed() {
  local root out
  root="$(fixture r1a)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/chromium/tabs_adapter.cc"
  out="$(check "$root")"
  expect "R1: an adapter may include chrome/browser/ui" "$out" $?
}

test_r2_view_by_class() {
  local root out status
  root="$(fixture r2)"
  echo 'void G(views::View* v) { views::AsViewClass<BrowserView>(v); }' \
    >"$root/src/browser/feature/walk.cc"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R2 src/browser/feature/walk.cc view-by-class<BrowserView>"* ]]
  expect "R2: a feature finding a Chromium view by class fails (its own classes pass)" "$status: $out" $?
}

test_r3_unregistered() {
  local root out status
  root="$(fixture r3u)"
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 '+  cromarc::hooks::X();'
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R3 H1 unregistered"* ]]
  expect "R3: an unregistered hook fails" "$status: $out" $?
}

test_r3_one_call_and_guard_pass() {
  local root out
  root="$(fixture r3ok)"
  printf '### H1 x\nKind: code\n' >>"$root/patches/hooks/README.md"
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 \
    "$(printf '%s\n' '+#include "cromarc/hooks/hooks.h"' '+  // Cromarc: why.' \
      '+  cromarc::hooks::X(a,' '+                    b);' \
      '+  if (cromarc::hooks::Takes(c)) {' '+    return;' '+  }' \
      '+  if (cromarc::hooks::Other(d)) return false;')"
  out="$(check "$root")"
  expect "R3: an include of hooks.h, a call and guards pass" "$out" $?
}

test_r3_logic_and_removed_lines() {
  local root out status
  root="$(fixture r3bad)"
  printf '### H1 x\nKind: code\n' >>"$root/patches/hooks/README.md"
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 \
    "$(printf '%s\n' '+#include "cromarc/browser/ui/sidebar/x.h"' '-  return false;' \
      '+  const bool space = cromarc::hooks::KeepsGlass(b);' '+  if (!space && n > 3) {')"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R3 H1 logic-line"* ]] &&
    [[ "$out" == *"R3 H1 removed-line"* ]] && [[ "$out" == *"R3 H1 include-not-hooks-h"* ]]
  expect "R3: logic, removed lines and other cromarc includes in a hook fail" "$status: $out" $?
}

test_r3_build_hook_free() {
  local root out
  root="$(fixture r3b)"
  printf '### H0 build\nKind: build\n' >>"$root/patches/hooks/README.md"
  hook_patch "$root/patches/hooks/0001-Cromarc-H0-x.patch" H0 "$(printf '%s\n' '-old' '+new')"
  out="$(check "$root")"
  expect "R3: a build hook may edit as it needs" "$out" $?
}

test_r4_system_change() {
  local root out status
  root="$(fixture r4)"
  printf '%s\n' 'void M() { [NSEvent addGlobalMonitorForEventsMatchingMask:0 handler:nil]; }' \
    'const char* p = "Library/Mobile Documents/x";' >"$root/src/browser/feature/sys.mm"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R4 src/browser/feature/sys.mm appkit-global"* ]] &&
    [[ "$out" == *"R4 src/browser/feature/sys.mm outside-profile"* ]]
  expect "R4: an unregistered system change fails, a path in a string too" "$status: $out" $?
  echo '| `src/browser/feature/sys.mm` | appkit-global outside-profile | test |' \
    >>"$root/docs/system-changes.md"
  out="$(check "$root")"
  expect "R4: the same change registered passes" "$out" $?
}

test_r4_window_material() {
  local root out status
  root="$(fixture r4m)"
  echo 'NSView* v = [[NSVisualEffectView alloc] init];' >"$root/src/browser/feature/m.mm"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R4 src/browser/feature/m.mm appkit-global"* ]]
  expect "R4: a material under a window outside its registered file fails" "$status: $out" $?
  echo 'NSView* g = [[NSGlassEffectView alloc] init];' >"$root/src/browser/feature/m.mm"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R4 src/browser/feature/m.mm appkit-global"* ]]
  expect "R4: glass under a window outside its registered file fails" "$status: $out" $?
}

test_baseline_ratchet() {
  local root out status
  root="$(fixture ratchet)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/feature/old.cc"
  check "$root" --update-baseline >/dev/null
  out="$(check "$root")"
  expect "baseline: a listed violation passes" "$out" $?
  printf '%s\n' '#include "chrome/browser/ui/browser.h"' '#include "chrome/browser/ui/x.h"' \
    >"$root/src/browser/feature/old.cc"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"chrome/browser/ui/x.h"* ]]
  expect "baseline: a new violation in a listed file fails" "$status: $out" $?
  out="$(check "$root" --update-baseline)"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"refusing to add"* ]]
  expect "baseline: --update-baseline refuses to grow" "$status: $out" $?
  echo '#include "base/x.h"' >"$root/src/browser/feature/old.cc"
  out="$(check "$root" --strict)"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"take them off the baseline"* ]]
  expect "baseline: --strict fails on a fixed but still listed violation" "$status: $out" $?
}

test_claude_hook() {
  local root out status
  root="$(fixture claude)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/feature/bad.cc"
  out="$(printf '{"tool_input":{"file_path":"%s"}}' "$root/src/browser/feature/bad.cc" |
    "$CHECK" --root "$root" --claude-hook 2>&1)"
  status=$?
  [ "$status" -eq 2 ] && [[ "$out" == *"R1"* ]] && [[ "$out" == *"AGENTS.md"* ]]
  expect "claude hook: a violation in the written file returns 2 with the rule" "$status: $out" $?
  out="$(printf '{"tool_input":{"file_path":"%s"}}' "$root/src/browser/feature/clean.cc" |
    "$CHECK" --root "$root" --claude-hook 2>&1)"
  expect "claude hook: a clean file passes" "$out" $?
}

test_baseline_grew_and_system_files() {
  local root out status
  root="$(fixture git)"
  git -C "$root" init -q
  git -C "$root" config user.email t@t.invalid
  git -C "$root" config user.name t
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/feature/old.cc"
  check "$root" --update-baseline >/dev/null
  git -C "$root" add -A
  git -C "$root" commit -q -m base
  echo 'R1 src/browser/feature/new.cc chrome/x.h' >>"$root/tooling/arch/baseline.txt"
  out="$(check "$root" --baseline-grew HEAD)"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"baseline grew"* ]]
  expect "baseline-grew: an added entry since REF fails" "$status: $out" $?
  echo '| `src/browser/feature/old.cc` | chrome-prefs | test |' >>"$root/docs/system-changes.md"
  hook_patch "$root/patches/hooks/0002-Cromarc-H2-y.patch" H2 '+  cromarc::hooks::Y();'
  echo '// x' >>"$root/src/browser/feature/old.cc"
  git -C "$root" add -A
  out="$(check "$root" --system-files)"
  [[ "$out" == *"patches/hooks/0002-Cromarc-H2-y.patch"* ]] &&
    [[ "$out" == *"src/browser/feature/old.cc"* ]] && [[ "$out" == *"docs/system-changes.md"* ]]
  expect "system-files: hooks, registered files and the registry need System-Change:" "$out" $?
}


# Each case: a registered code hook adding BODY must report R3 logic-line.
r3_logic_case() {
  local name="$1" body="$2" root out status
  root="$(fixture "r3case$RANDOM")"
  printf '### H1 x\nKind: code\n' >>"$root/patches/hooks/README.md"
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 "$body"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R3 H1 logic-line"* ]]
  expect "R3: $name" "$status: $out" $?
}

test_r3_logic_hidden_by_a_hook_call() {
  r3_logic_case "logic after a hook call on its line" '+  cromarc::hooks::Do(a); danger();'
  r3_logic_case "logic on a hook call's continuation line" \
    "$(printf '%s\n' '+  cromarc::hooks::Do(a,' '+      b); if (x) { y(); }')"
  r3_logic_case "a hook named only in a comment" '+  /* cromarc::hooks::X */ evil();'
  r3_logic_case "a hook call inside an expression" '+  int q = (cromarc::hooks::Adjust(a, b ? c : d));'
  r3_logic_case "a hook call with more after it" '+  x = cromarc::hooks::Adjust(a) + extra();'
  r3_logic_case "a guard whose body does more than return" \
    '+  if (cromarc::hooks::Handles(a)) { x = 1; return; }'
  r3_logic_case "an added ++i (not a file header)" '+++i;'
}

test_r3_wrapped_values() {
  local root out status
  root="$(fixture r3wrap)"
  printf '### H1 x\nKind: code\n' >>"$root/patches/hooks/README.md"
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 \
    "$(printf '%s\n' '-  const CGFloat a = GetTint(x);' \
      '+  const CGFloat a = cromarc::hooks::GlassTint(view, GetTint(x));' \
      '-BASE_FEATURE(kGlass, base::FEATURE_DISABLED_BY_DEFAULT);' \
      '+BASE_FEATURE(kGlass, cromarc::hooks::GlassDefault(base::FEATURE_DISABLED_BY_DEFAULT));' \
      '-    if (combo_button_) {' '+    if (cromarc::hooks::TakesRoom(combo_button_)) {')"
  out="$(check "$root")"
  expect "R3: a Chromium value wrapped in a hook call passes" "$out" $?
  hook_patch "$root/patches/hooks/0001-Cromarc-H1-x.patch" H1 \
    "$(printf '%s\n' '-  return false;' '+  return true;' \
      '-  if (a < b) {' '+  if (a <= cromarc::hooks::X(b)) {')"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R3 H1 logic-line"* ]] && [[ "$out" == *"R3 H1 removed-line"* ]]
  expect "R3: a changed value or operator around a hook call fails" "$status: $out" $?
}

test_r3_code_hook_build_files() {
  local root out status
  root="$(fixture r3gn)"
  printf '### H1 x\nKind: code\n' >>"$root/patches/hooks/README.md"
  {
    printf 'Subject: [PATCH] Cromarc H1: test\n\n'
    printf 'diff --git a/chrome/browser/ui/BUILD.gn b/chrome/browser/ui/BUILD.gn\n'
    printf -- '--- a/chrome/browser/ui/BUILD.gn\n+++ b/chrome/browser/ui/BUILD.gn\n@@ -1,2 +1,3 @@\n'
    printf '%s\n' '+      "//cromarc:headers",  # Cromarc'
  } >"$root/patches/hooks/0001-Cromarc-H1-x.patch"
  out="$(check "$root")"
  expect "R3: a code hook may add a //cromarc dependency to a build file" "$out" $?
  printf '%s\n' '+  deps += [ "//foo" ]' >>"$root/patches/hooks/0001-Cromarc-H1-x.patch"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R3 H1 build-edit"* ]]
  expect "R3: a code hook may not edit build files otherwise" "$status: $out" $?
}

test_r1_r2_forms() {
  local root out status
  root="$(fixture r1forms)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/library_ui.cc"
  printf '%s\n' '#include <chrome/browser/x.h>' '#include "base/../chrome/y.h"' \
    'void G(views::View* v) { views::AsViewClass<' '    BrowserView>(v)->contents(); }' \
    >"$root/src/browser/feature/forms.cc"
  out="$(check "$root")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"R1 src/browser/library_ui.cc"* ]] &&
    [[ "$out" == *"R1 src/browser/feature/forms.cc chrome/browser/x.h"* ]] &&
    [[ "$out" == *"R1 src/browser/feature/forms.cc base/../chrome/y.h"* ]] &&
    [[ "$out" == *"R2 src/browser/feature/forms.cc view-by-class<BrowserView>"* ]] &&
    [[ "$out" == *"R2 src/browser/feature/forms.cc BrowserView"* ]]
  expect "R1/R2: a *_ui.cc outside ui/*/, <chrome/…>, ../, multi-line view search all fail" \
    "$status: $out" $?
}

test_claude_hook_relative_path() {
  local root out status
  root="$(fixture claude-rel)"
  echo '#include "chrome/browser/ui/browser.h"' >"$root/src/browser/feature/bad.cc"
  out="$(printf '{"cwd":"%s","tool_input":{"file_path":"src/browser/feature/bad.cc"}}' "$root" |
    "$CHECK" --root "$root" --claude-hook 2>&1)"
  status=$?
  [ "$status" -eq 2 ]
  expect "claude hook: a path relative to the event's cwd is checked" "$status: $out" $?
}

test_update_baseline_whole_repo_only() {
  local root out status
  root="$(fixture ub)"
  out="$(check "$root" --update-baseline --files "$root/src/browser/feature/clean.cc")"
  status=$?
  [ "$status" -eq 1 ] && [[ "$out" == *"whole repository only"* ]]
  expect "baseline: --update-baseline refuses a subset of files" "$status: $out" $?
}

test_clean_passes
test_r1_feature_include
test_r1_own_resources_allowed
test_r1_foundation_allowed
test_r1_adapter_allowed
test_r2_view_by_class
test_r3_unregistered
test_r3_one_call_and_guard_pass
test_r3_logic_and_removed_lines
test_r3_build_hook_free
test_r4_system_change
test_r4_window_material
test_baseline_ratchet
test_claude_hook
test_baseline_grew_and_system_files
test_r3_logic_hidden_by_a_hook_call
test_r3_wrapped_values
test_r3_code_hook_build_files
test_r1_r2_forms
test_claude_hook_relative_path
test_update_baseline_whole_repo_only

printf '%d passed, %d failed\n' "$passes" "$failures"
[ "$failures" -eq 0 ]
