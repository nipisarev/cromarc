#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Tests for tooling/apply and tooling/export-patches against a throwaway git tree
# that stands in for chromium/src. Run: tooling/tests/test_patches.sh

set -uo pipefail

TOOLING_DIR="$(cd "$(dirname "$0")/.." && pwd)"
failures=0
passes=0

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

# A fake Cromarc repository (personal identity, tooling linked in) and a fake
# Chromium tree (work identity, tag 1.0.0.0). Sets $repo and $src.
setup() {
  local tmp
  tmp="$(cd "$(mktemp -d)" && pwd -P)"
  repo="$tmp/cromarc"
  src="$tmp/chromium/src"
  mkdir -p "$repo/src/browser" "$repo/patches/hooks" "$src"
  ln -s "$TOOLING_DIR" "$repo/tooling"
  printf 'CHROMIUM_TAG=1.0.0.0\nCHROMIUM_MILESTONE=1\nCHROMIUM_COMMIT=0\nCHROMIUM_CHANNEL=stable\nCHROMIUM_PIN_DATE=2026-01-01\n' \
    > "$repo/CHROMIUM_VERSION"
  echo 'ours' > "$repo/src/browser/ours.cc"
  git -C "$repo" init -q
  git -C "$repo" config user.email personal@example.com
  git -C "$repo" config user.name 'Personal Name'

  git -C "$src" init -q
  git -C "$src" config user.email work@corp.example
  git -C "$src" config user.name 'Work Name'
  printf 'one\ntwo\nthree\n' > "$src/upstream.cc"
  git -C "$src" add upstream.cc
  git -C "$src" commit -q -m 'upstream'
  git -C "$src" tag 1.0.0.0
}

teardown() {
  rm -rf "$(dirname "$repo")"
}

run() {
  CROMARC_ROOT="$repo" CHROMIUM_SRC="$src" "$repo/tooling/$1" 2>&1
}

# Commits a one-line change to upstream.cc on cromarc-work, as the personal identity.
hook_commit() {
  sed -i '' "s/^$1\$/$1 (cromarc)/" "$src/upstream.cc"
  git -C "$src" commit -q -am "Hook: $1"
}

test_apply_mirrors_src_and_branches_from_the_pin() {
  local out status
  setup
  out="$(run apply)"; status=$?
  [[ "$status" -eq 0 \
    && "$(cat "$src/cromarc/browser/ours.cc")" = ours \
    && "$(git -C "$src" rev-parse --abbrev-ref HEAD)" = cromarc-work \
    && "$(git -C "$src" rev-parse HEAD)" = "$(git -C "$src" rev-parse 1.0.0.0)" \
    && -z "$(git -C "$src" status --porcelain)" ]]
  expect "apply: src/ lands in cromarc/, cromarc-work starts at the pin, tree stays clean" \
    "status=$status out=$out" $?
  teardown
}

test_apply_gives_the_tree_the_repo_identity() {
  local email
  setup
  run apply >/dev/null
  email="$(git -C "$src" config user.email)"
  [ "$email" = personal@example.com ]
  expect "apply: commits in the Chromium tree use the repo's email, not the global one" \
    "got: $email" $?
  teardown
}

test_export_then_apply_round_trips() {
  local out status patches
  setup
  run apply >/dev/null
  hook_commit two
  run export-patches >/dev/null
  rm -rf "$src/cromarc"
  out="$(run apply)"; status=$?
  patches="$(ls "$repo/patches/hooks")"
  [[ "$status" -eq 0 \
    && "$patches" = "0001-Hook-two.patch" \
    && "$(git -C "$src" rev-list --count 1.0.0.0..cromarc-work)" = 1 \
    && "$(sed -n 2p "$src/upstream.cc")" = "two (cromarc)" \
    && -f "$src/cromarc/browser/ours.cc" ]]
  expect "export-patches + apply: the hook comes back as one commit on the pin" \
    "status=$status out=$out patches=$patches" $?
  teardown
}

test_apply_refuses_unexported_commits() {
  local out status
  setup
  run apply >/dev/null
  hook_commit three
  out="$(run apply)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"tooling/export-patches"* \
    && "$(git -C "$src" rev-list --count 1.0.0.0..cromarc-work)" = 1 ]]
  expect "apply: a commit missing from patches/hooks is not thrown away" "status=$status out=$out" $?
  teardown
}

test_apply_refuses_a_dirty_tree() {
  local out status
  setup
  run apply >/dev/null
  echo 'stray' >> "$src/upstream.cc"
  out="$(run apply)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"uncommitted"* ]]
  expect "apply: uncommitted edits in the Chromium tree stop it" "status=$status out=$out" $?
  teardown
}

test_apply_drops_files_removed_from_src() {
  local before after
  setup
  echo 'old' > "$repo/src/browser/old.cc"
  run apply >/dev/null
  [ -f "$src/cromarc/browser/old.cc" ] && before=present || before=absent
  rm "$repo/src/browser/old.cc"
  run apply >/dev/null
  [ -e "$src/cromarc/browser/old.cc" ] && after=present || after=absent
  [[ "$before" = present && "$after" = absent ]]
  expect "apply: a file deleted from src/ disappears from cromarc/" "before=$before after=$after" $?
  teardown
}

test_export_refuses_a_foreign_author() {
  local out status
  setup
  run apply >/dev/null
  sed -i '' 's/^one$/one (cromarc)/' "$src/upstream.cc"
  git -C "$src" -c user.email=work@corp.example commit -q -am 'Hook: one'
  out="$(run export-patches)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"work@corp.example"* && -z "$(ls "$repo/patches/hooks")" ]]
  expect "export-patches: a commit by another address never reaches patches/hooks" "status=$status out=$out" $?
  teardown
}

echo "tooling/apply, tooling/export-patches"
test_apply_mirrors_src_and_branches_from_the_pin
test_apply_gives_the_tree_the_repo_identity
test_export_then_apply_round_trips
test_apply_refuses_unexported_commits
test_apply_refuses_a_dirty_tree
test_apply_drops_files_removed_from_src
test_export_refuses_a_foreign_author

printf '\n%d passed, %d failed\n' "$passes" "$failures"
[ "$failures" -eq 0 ]
