#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Tests for the CI checks: tooling/check-patches, tooling/check-headers,
# tooling/forecast and .github/workflows/check.yml. Gitiles and chromiumdash
# are a local stand-in (gitiles_standin.py) serving a throwaway Chromium, so
# nothing goes to the network. Run: tooling/tests/test_ci.sh

set -uo pipefail

# Run from a git hook, git's own variables point at this repository and its
# commit; the throwaway repositories below must not see them.
while read -r var; do unset "$var"; done < <(git rev-parse --local-env-vars)
unset GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_AUTHOR_DATE \
  GIT_COMMITTER_NAME GIT_COMMITTER_EMAIL GIT_COMMITTER_DATE

TOOLING_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WORKFLOW="$TOOLING_DIR/../.github/workflows/check.yml"
failures=0
passes=0

expect() {
  local name="$1" detail="$2" status="$3"
  if [ "$status" -eq 0 ]; then
    passes=$((passes + 1))
    printf '  ok   %s\n' "$name"
  else
    failures=$((failures + 1))
    # Indented, so the checks' own "  ok" lines don't read as results.
    printf '  FAIL %s\n' "$name"
    printf '%s\n' "$detail" | sed 's/^/       /'
  fi
}

tmp="$(cd "$(mktemp -d)" && pwd -P)"
standin=""
cleanup() {
  if [ -n "$standin" ]; then
    kill "$standin" 2>/dev/null
    wait "$standin" 2>/dev/null
  fi
  rm -rf "$tmp"
}
trap cleanup EXIT

# The user's git config stays out; no background maintenance in the fixtures.
export GIT_CONFIG_GLOBAL="$tmp/gitconfig" GIT_CONFIG_NOSYSTEM=1
printf '[maintenance]\n\tauto = false\n[gc]\n\tauto = 0\n[user]\n\temail = upstream@example.com\n\tname = Upstream\n[init]\n\tdefaultBranch = main\n' \
  > "$GIT_CONFIG_GLOBAL"
export NO_PROXY=127.0.0.1 no_proxy=127.0.0.1

# Upstream Chromium. 1.0.0.0 is the pin; 1.0.0.9, 1.0.0.10 and 1.5.0.0 are
# the same tree. 2.0.0.0 changed the hooked line and dropped base/foo.h;
# 3.0.0.0 dropped chrome/browser/.
upstream="$tmp/upstream"
git init -q "$upstream"
mkdir -p "$upstream/chrome/browser" "$upstream/base"
printf 'one\ntwo\nthree\n' > "$upstream/chrome/browser/hooked.cc"
echo '// foo' > "$upstream/base/foo.h"
echo '// other' > "$upstream/base/other.h"
echo '// x' > "$upstream/chrome/browser/x.h"
git -C "$upstream" add -A
git -C "$upstream" commit -q -m 1.0.0.0
for tag in 1.0.0.0 1.0.0.9 1.0.0.10 1.5.0.0; do git -C "$upstream" tag "$tag"; done

# Cromarc: tooling linked in, two hooks (one changes a line, one adds a file)
# exported from the pin as tooling/export-patches writes them.
repo="$tmp/cromarc"
mkdir -p "$repo/src/browser" "$repo/patches/hooks"
ln -s "$TOOLING_DIR" "$repo/tooling"
cat > "$repo/src/browser/feature.cc" <<'EOF'
#include "base/foo.h"
#include "chrome/browser/x.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "cromarc/browser/feature.h"
EOF
git -C "$upstream" checkout -q -b cromarc-work
printf 'one\ntwo (cromarc)\nthree\n' > "$upstream/chrome/browser/hooked.cc"
git -C "$upstream" commit -q -am 'Hook: two'
echo 'new' > "$upstream/cromarc_new.gn"
git -C "$upstream" add cromarc_new.gn
git -C "$upstream" commit -q -m 'Hook: a new file'
git -C "$upstream" format-patch -q -o "$repo/patches/hooks" 1.0.0.0
git -C "$upstream" checkout -q main
printf 'one\n2\nthree\n' > "$upstream/chrome/browser/hooked.cc"
git -C "$upstream" rm -q base/foo.h
git -C "$upstream" commit -q -am 2.0.0.0
git -C "$upstream" tag 2.0.0.0
git -C "$upstream" checkout -q -b three 1.0.0.0
git -C "$upstream" rm -rq chrome/browser
git -C "$upstream" commit -q -m 3.0.0.0
git -C "$upstream" tag 3.0.0.0

state="$tmp/state"
mkdir -p "$state"
python3 "$TOOLING_DIR/tests/gitiles_standin.py" "$upstream" "$state" > "$tmp/port" &
standin=$!
for _ in $(seq 100); do [ -s "$tmp/port" ] && break; sleep 0.1; done
if [ ! -s "$tmp/port" ]; then
  kill -0 "$standin" 2>/dev/null && how="still starting" || how="exited"
  echo "the Gitiles stand-in did not start within 10 s ($how)"
  exit 1
fi
base="http://127.0.0.1:$(cat "$tmp/port")"
export CHROMIUM_URL="$upstream" CHROMIUM_GITILES="$base" CHROMIUM_DASH="$base"

# A fresh start for a test: the pin ($1, default 1.0.0.0), an empty TMPDIR
# for the checks, no releases, faults or requests.
fresh() {
  printf 'CHROMIUM_TAG=%s\nCHROMIUM_MILESTONE=1\nCHROMIUM_COMMIT=0\nCHROMIUM_CHANNEL=stable\nCHROMIUM_PIN_DATE=2026-01-01\n' \
    "${1:-1.0.0.0}" > "$repo/CHROMIUM_VERSION"
  rm -rf "$tmp/scratch"
  mkdir -p "$tmp/scratch"
  rm -f "$state/releases.json" "$state/faults.json"
  : > "$state/requests.log"
}

run() {
  CROMARC_ROOT="$repo" TMPDIR="$tmp/scratch" "$repo/tooling/$1" "${@:2}" 2>&1
}

scratch_empty() { [ -z "$(ls -A "$tmp/scratch")" ]; }
requests() { grep -cxF "$1" "$state/requests.log"; }
fetched_tags() { grep -c '^/+/refs/tags/' "$state/requests.log"; }

test_check_patches_hooks_that_apply_pass() {
  local out status
  fresh
  out="$(run check-patches)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"ok        0001-Hook-two.patch"* \
    && "$out" == *"ok        0002-Hook-a-new-file.patch"* && "$out" == *"patches/hooks apply to 1.0.0.0"* \
    && "$(requests /+/refs/tags/1.0.0.0/chrome/browser/hooked.cc)" = 1 \
    && "$(requests /+/refs/tags/1.0.0.0/cromarc_new.gn)" = 0 ]] && scratch_empty
  expect "check-patches: hooks that apply pass, one line per patch; added files are not fetched" \
    "status=$status out=$out" $?
}

test_check_patches_names_a_hook_that_does_not_apply() {
  local out status
  fresh
  out="$(run check-patches 2.0.0.0)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"CONFLICT  0001-Hook-two.patch"* \
    && "$out" == *"ok        0002-Hook-a-new-file.patch"* \
    && "$out" == *"patches/hooks do not all apply to 2.0.0.0"* ]] && scratch_empty
  expect "check-patches: a hook that does not apply fails, naming it, and the rest are still checked" \
    "status=$status out=$out" $?
}

test_check_patches_names_a_file_gone_at_the_tag() {
  local out status
  fresh
  out="$(run check-patches 3.0.0.0)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"chrome/browser/hooked.cc is not in 3.0.0.0"* ]] && scratch_empty
  expect "check-patches: a hooked file gone at the tag fails, naming the file" "status=$status out=$out" $?
}

test_check_patches_refuses_a_tag_chromium_does_not_have() {
  local out status
  fresh
  out="$(run check-patches 9.9.9.9)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"Chromium has no tag 9.9.9.9"* && "$(fetched_tags)" = 0 ]] && scratch_empty
  expect "check-patches: a tag Chromium does not have is refused before any fetch" "status=$status out=$out" $?
}

test_check_patches_refuses_a_malformed_tag() {
  local out status
  fresh
  out="$(run check-patches 157.0-beta)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"a Chromium version tag"* && "$(fetched_tags)" = 0 ]]
  expect "check-patches: a malformed tag is refused" "status=$status out=$out" $?
}

test_check_patches_retries_a_busy_gitiles() {
  local out status
  fresh
  echo '{"/+/refs/tags/1.0.0.0/chrome/browser/hooked.cc": [503]}' > "$state/faults.json"
  out="$(run check-patches)"; status=$?
  [[ "$status" -eq 0 && "$(requests /+/refs/tags/1.0.0.0/chrome/browser/hooked.cc)" = 2 ]] && scratch_empty
  expect "check-patches: Gitiles busy for a moment (503) is retried" "status=$status out=$out" $?
}

test_check_patches_fails_on_a_refused_fetch() {
  local out status
  fresh
  echo '{"/+/refs/tags/1.0.0.0/chrome/browser/hooked.cc": [403]}' > "$state/faults.json"
  out="$(run check-patches)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"fetching chrome/browser/hooked.cc at 1.0.0.0 failed (HTTP 403)"* ]] && scratch_empty
  expect "check-patches: a fetch Gitiles refuses fails with its HTTP code" "status=$status out=$out" $?
}

# The race behind CI's "Directory not empty": git starts maintenance in the
# background after a commit or git am, still writing into the scratch
# repository while the exit trap removes it. A global config asking for it
# after every commit must not reach the scratch repository.
test_check_patches_starts_no_background_maintenance() {
  local out status spawned
  fresh
  printf '[maintenance]\n\tauto = true\n[gc]\n\tauto = 1\n' > "$tmp/gitconfig-maintenance"
  out="$(GIT_CONFIG_GLOBAL="$tmp/gitconfig-maintenance" GIT_TRACE2_EVENT="$tmp/trace.json" run check-patches)"
  status=$?
  spawned="$(grep '"event":"child_start"' "$tmp/trace.json" | grep -E '"(maintenance|gc)"')"
  rm -f "$tmp/trace.json"
  [[ "$status" -eq 0 && -z "$spawned" ]] && scratch_empty
  expect "check-patches: its scratch repository starts no background git maintenance" \
    "status=$status spawned=$spawned out=$out" $?
}

test_check_headers_all_present_pass() {
  local out status
  fresh
  out="$(run check-headers)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"all 2 included Chromium headers are in 1.0.0.0"* ]]
  expect "check-headers: every included Chromium header present passes; generated, DEPS and own headers are not counted" \
    "status=$status out=$out" $?
}

test_check_headers_lists_a_header_gone() {
  local out status
  fresh
  out="$(run check-headers 2.0.0.0)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"1 of 2 included Chromium headers are not in 2.0.0.0"* \
    && "$out" == *"  base/foo.h"* && "$out" != *"chrome/browser/x.h"* ]]
  expect "check-headers: a header gone at the tag is listed and fails" "status=$status out=$out" $?
}

test_check_headers_lists_the_headers_of_a_directory_gone() {
  local out status
  fresh
  out="$(run check-headers 3.0.0.0)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"1 of 2 included Chromium headers are not in 3.0.0.0"* \
    && "$out" == *"  chrome/browser/x.h"* ]]
  expect "check-headers: the headers of a directory gone at the tag are listed" "status=$status out=$out" $?
}

test_check_headers_refuses_a_malformed_tag() {
  local out status
  fresh
  out="$(run check-headers 1.0-beta)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"a Chromium version tag"* && "$(fetched_tags)" = 0 ]]
  expect "check-headers: a malformed tag is refused" "status=$status out=$out" $?
}

test_check_headers_retries_a_busy_gitiles() {
  local out status
  fresh
  echo '{"/+/refs/tags/1.0.0.0/base/": [503]}' > "$state/faults.json"
  out="$(run check-headers)"; status=$?
  [[ "$status" -eq 0 && "$(requests /+/refs/tags/1.0.0.0/base/)" = 2 ]]
  expect "check-headers: Gitiles busy for a moment (503) is retried" "status=$status out=$out" $?
}

test_forecast_up_to_date_checks_nothing() {
  local out status
  fresh
  echo '{"Stable": "1.0.0.0"}' > "$state/releases.json"
  out="$(run forecast Stable)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"up to date"* && "$(fetched_tags)" = 0 ]]
  expect "forecast: the channel's newest release equal to the pin checks nothing" "status=$status out=$out" $?
}

test_forecast_pin_ahead_checks_nothing() {
  local out status
  fresh
  echo '{"Beta": "0.9.0.0"}' > "$state/releases.json"
  out="$(run forecast Beta)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"the pin is ahead of Mac Beta"* && "$(fetched_tags)" = 0 ]]
  expect "forecast: a pin ahead of the channel checks nothing" "status=$status out=$out" $?
}

test_forecast_newer_release_that_holds_passes() {
  local out status
  fresh
  echo '{"Stable": "1.5.0.0"}' > "$state/releases.json"
  out="$(run forecast Stable)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"::notice::Chrome Stable 1.5.0.0 is out"* \
    && "$out" == *"patches/hooks apply to 1.5.0.0"* \
    && "$out" == *"all 2 included Chromium headers are in 1.5.0.0"* && "$out" != *"::warning::"* ]]
  expect "forecast: a newer release that takes the hooks and has the headers passes" "status=$status out=$out" $?
}

test_forecast_stable_failure_fails() {
  local out status
  fresh
  echo '{"Stable": "2.0.0.0"}' > "$state/releases.json"
  out="$(run forecast Stable)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"CONFLICT  0001-Hook-two.patch"* && "$out" == *"  base/foo.h"* \
    && "$out" != *"::warning::"* ]]
  expect "forecast: on Stable a hook that does not apply or a header gone fails, with no warning" \
    "status=$status out=$out" $?
}

test_forecast_beta_and_dev_failures_warn() {
  local out status channel result=0 detail=""
  for channel in Beta Dev; do
    fresh
    echo "{\"$channel\": \"2.0.0.0\"}" > "$state/releases.json"
    out="$(run forecast "$channel")"; status=$?
    [[ "$status" -eq 1 && "$out" == *"::warning::hooks or headers need work before Chrome 2.0.0.0"* ]] ||
      { result=1; detail="$detail $channel: status=$status out=$out"; }
  done
  expect "forecast: on Beta and Dev a failure warns" "$detail" "$result"
}

test_forecast_compares_versions_as_numbers() {
  local out status
  fresh 1.0.0.9
  echo '{"Stable": "1.0.0.10"}' > "$state/releases.json"
  out="$(run forecast Stable)"; status=$?
  [[ "$status" -eq 0 && "$out" == *"::notice::Chrome Stable 1.0.0.10 is out"* \
    && "$out" == *"patches/hooks apply to 1.0.0.10"* ]]
  expect "forecast: versions compare as numbers (1.0.0.10 is newer than 1.0.0.9)" "status=$status out=$out" $?
}

test_forecast_refuses_an_unknown_channel() {
  local out status channel result=0 detail=""
  for channel in Canary ''; do
    fresh
    out="$(run forecast "$channel")"; status=$?
    [[ "$status" -eq 1 && "$out" == *"Stable, Beta or Dev"* && ! -s "$state/requests.log" ]] ||
      { result=1; detail="$detail '$channel': status=$status out=$out"; }
  done
  expect "forecast: a channel other than Stable, Beta or Dev is refused" "$detail" "$result"
}

test_forecast_fails_without_a_release() {
  local out status
  fresh
  echo '{}' > "$state/releases.json"
  out="$(run forecast Stable)"; status=$?
  [[ "$status" -eq 1 && "$out" == *"chromiumdash named no Stable release for Mac"* && "$(fetched_tags)" = 0 ]]
  expect "forecast: chromiumdash naming no release fails" "status=$status out=$out" $?
}

# The workflow, read as YAML by the system ruby (YAML 1.1 reads `on:` as true).
workflow_problems() {
  ruby --disable-gems -ryaml - "$WORKFLOW" "$1" <<'RUBY'
w = YAML.load_file(ARGV[0])
on = w['on'] || w[true]
lines = ->(job) { w['jobs'][job]['steps'].map { |s| s['run'] }.compact.join("\n").lines.map(&:strip) }
problems = []
case ARGV[1]
when 'push'
  problems << "push is to #{on.dig('push', 'branches').inspect}, not [main]" unless on.dig('push', 'branches') == ['main']
  ran = lines.('repository')
  ['tooling/check', 'tooling/arch-check --strict', 'tooling/check-patches'].each do |c|
    problems << "the repository job does not run #{c}" unless ran.include?(c)
  end
  ['tooling/arch-check --baseline-grew ', 'tooling/arch-check --check-commits '].each do |c|
    problems << "the repository job does not run #{c.strip}" unless ran.any? { |l| l.start_with?(c) }
  end
when 'daily'
  newest = w['jobs']['newest']
  problems << 'no daily schedule' unless (on['schedule'] || []).any? { |s| s['cron'] }
  channels = newest.dig('strategy', 'matrix', 'channel')
  problems << "the channels are #{channels.inspect}" unless channels == %w[Stable Beta Dev]
  problems << "continue-on-error is #{newest['continue-on-error'].inspect}" unless
    newest['continue-on-error'] == "${{ matrix.channel != 'Stable' }}"
  problems << 'the newest job does not run tooling/forecast' unless
    lines.('newest').include?('tooling/forecast "${{ matrix.channel }}"')
end
puts problems
exit problems.empty?
RUBY
}

test_workflow_push_runs_the_checks() {
  local problems
  problems="$(workflow_problems push 2>&1)"
  expect "CI: every push to main runs tooling/check, the strict architecture check and check-patches on the pin" \
    "$problems" $?
}

test_workflow_daily_forecast_fails_only_on_stable() {
  local problems
  problems="$(workflow_problems daily 2>&1)"
  expect "CI: every day it forecasts Stable, Beta and Dev, and only Stable fails the run" "$problems" $?
}

echo "tooling/check-patches, tooling/check-headers, tooling/forecast, the CI workflow"
test_check_patches_hooks_that_apply_pass
test_check_patches_names_a_hook_that_does_not_apply
test_check_patches_names_a_file_gone_at_the_tag
test_check_patches_refuses_a_tag_chromium_does_not_have
test_check_patches_refuses_a_malformed_tag
test_check_patches_retries_a_busy_gitiles
test_check_patches_fails_on_a_refused_fetch
test_check_patches_starts_no_background_maintenance
test_check_headers_all_present_pass
test_check_headers_lists_a_header_gone
test_check_headers_lists_the_headers_of_a_directory_gone
test_check_headers_refuses_a_malformed_tag
test_check_headers_retries_a_busy_gitiles
test_forecast_up_to_date_checks_nothing
test_forecast_pin_ahead_checks_nothing
test_forecast_newer_release_that_holds_passes
test_forecast_stable_failure_fails
test_forecast_beta_and_dev_failures_warn
test_forecast_compares_versions_as_numbers
test_forecast_refuses_an_unknown_channel
test_forecast_fails_without_a_release
if command -v ruby >/dev/null; then
  test_workflow_push_runs_the_checks
  test_workflow_daily_forecast_fails_only_on_stable
else
  echo "ruby not installed; the workflow checks skipped" >&2
  [ -z "${CI:-}" ] || failures=$((failures + 1))
fi

printf '\n%d passed, %d failed\n' "$passes" "$failures"
[ "$failures" -eq 0 ]
