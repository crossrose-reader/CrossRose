#!/usr/bin/env bash
# Make a CrossRose stable release.
#
# Usage: ./scripts/release.sh <version> <next-version>
#   ./scripts/release.sh 0.1.0 0.2.0    on main
#   ./scripts/release.sh 0.1.1 0.1.2    on release/0.1 (a fix for an older version)
#
# Versions are MAJOR.MINOR.PATCH ("semantic versioning"):
#   0.2.0 -> 0.3.0  new features (MINOR goes up)
#   0.2.0 -> 0.2.1  only bug fixes (PATCH goes up)
#   1.0.0           the first big release; 0.x versions lead up to it
#
# What it does (on your computer only, nothing is pushed):
#   1. Checks you are on main (or a release/MAJOR.MINOR branch), with no
#      uncommitted changes, and the same as GitHub.
#   2. Checks <version> is new and in the right place:
#      - It is not lower than the version in platformio.ini (the version this
#        branch is working toward). To really release a lower one, first
#        lower [crossink] version in a commit of its own.
#      - On main it must be higher than every release on GitHub.
#      - A release/MAJOR.MINOR branch only makes fixes for a line that is
#        already out: release/0.1 needs a v0.1.x release on GitHub, its
#        platformio.ini must say 0.1.x, both versions you type must be 0.1.x,
#        and <version> must be higher than every 0.1.x release.
#   3. Renames "## [Unreleased]" in CHANGELOG.md to "## [v<version>] - <today>".
#   4. Sets [crossink] version in platformio.ini to <version>.
#   5. Commits, and makes the tag v<version>.
#   6. A second commit sets the version to <next-version> and starts a new
#      "## [Unreleased]" section. So platformio.ini always names a version
#      that is not out yet: nightly builds are named after it, and a build
#      made from source counts as a test build of it. Otherwise devices on a
#      nightly would be offered an older stable version as an "update".
#
# Then push the branch and the tag together. The tag starts the Release
# workflow on GitHub, which builds the firmware and makes a draft release.
#
# Works with bash 3.2 (macOS) and newer, and with BSD or GNU tools
# (macOS, Linux, WSL). Git Bash on Windows works only if this file and
# scripts/crossrose_version.sh are checked out with LF line endings; with
# Windows (CRLF) endings bash stops at the first line.
set -euo pipefail

usage() {
  echo "Usage: $0 <version> <next-version>   (example: $0 0.1.0 0.2.0)" >&2
  exit 1
}
[[ $# -eq 2 ]] || usage
VERSION="$1"
NEXT="$2"

cd "$(git rev-parse --show-toplevel)"

# All version rules (format, order, reading and writing platformio.ini) live
# in one helper that the GitHub workflows use too.
versions() {
  bash scripts/crossrose_version.sh "$@"
}

if ! versions valid "$VERSION" || ! versions valid "$NEXT"; then
  usage
fi

fail() {
  echo "Stopped: $1" >&2
  exit 1
}

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
LINE="" # MAJOR.MINOR on a release/ branch, empty on main
if [[ "$BRANCH" == release/* ]]; then
  LINE="${BRANCH#release/}"
  # 0.1 is a valid line exactly when 0.1.0 is a valid version.
  versions valid "$LINE.0" ||
    fail "release branches are named release/MAJOR.MINOR (like release/0.1), not '$BRANCH'."
elif [[ "$BRANCH" != "main" ]]; then
  fail "you are on '$BRANCH'. Releases are made from main (or a release/MAJOR.MINOR branch)."
fi
[[ -z "$(git status --porcelain --untracked-files=no)" ]] ||
  fail "you have uncommitted changes. Commit or stash them first."
git fetch --quiet origin
[[ "$(git rev-parse HEAD)" == "$(git rev-parse "origin/$BRANCH")" ]] ||
  fail "your $BRANCH is not the same as GitHub's. Run 'git pull' (or push) first."
! git rev-parse -q --verify "refs/tags/v$VERSION" >/dev/null ||
  fail "the tag v$VERSION already exists."
grep -q '^## \[Unreleased\]' CHANGELOG.md ||
  fail "CHANGELOG.md has no '## [Unreleased]' section."
versions gt "$NEXT" "$VERSION" ||
  fail "the next version ($NEXT) must be higher than $VERSION."

# The version this branch is working toward. Stops if it is missing or bad.
CURRENT="$(versions ini)"

# A release/0.1 branch makes 0.1.x fixes and nothing else. Otherwise a typo
# (0.2.1 for 0.1.2) would tag the old 0.1 code as the newest version, and
# devices on 0.2.0 would be offered it as an "update".
if [[ -n "$LINE" ]]; then
  [[ "${CURRENT%.*}" == "$LINE" ]] ||
    fail "platformio.ini on $BRANCH says $CURRENT, which is not a $LINE.x version. Make $BRANCH from the v$LINE.x tag you want to fix."
  for v in "$VERSION" "$NEXT"; do
    [[ "${v%.*}" == "$LINE" ]] ||
      fail "$BRANCH only makes $LINE.x releases, and $v is not one."
  done
fi

# Never go below the version platformio.ini is working toward. Nightlies may
# already be out under that version, and devices on them would never be
# offered a lower one.
! versions gt "$CURRENT" "$VERSION" ||
  fail "$VERSION is lower than $CURRENT, the version in platformio.ini on $BRANCH. To really release a lower version, first lower [crossink] version in a commit of its own."

# The releases that exist are the v* tags on GitHub. Ask GitHub directly, so
# tags that are only on this computer (for example CrossInk's, if you fetched
# tags from the upstream remote) do not count.
REMOTE_TAGS="$(git ls-remote --tags --refs origin)" ||
  fail "could not read the tags on GitHub."
if [[ -z "$LINE" ]]; then
  NEWEST="$(printf '%s\n' "$REMOTE_TAGS" | cut -f2 | versions highest)"
  WHICH="the newest release"
else
  NEWEST="$(printf '%s\n' "$REMOTE_TAGS" | cut -f2 | versions highest "$LINE")"
  WHICH="the newest $LINE.x release"
  # Release branches are for fixes to a line that is already out. Releasing
  # its first version here would take main's target: main's nightly would
  # then stop with "already released", and main could not release it.
  [[ -n "$NEWEST" ]] ||
    fail "$BRANCH is for fixes to $LINE.x, but no $LINE.x release is on GitHub yet. Release $LINE.0 from main."
fi
if [[ -n "$NEWEST" ]] && ! versions gt "$VERSION" "$NEWEST"; then
  fail "$VERSION must be higher than v$NEWEST, $WHICH on GitHub."
fi

# Replaces the first "## [Unreleased]" heading. Writes a new file and moves it
# into place, which works the same with BSD (macOS) and GNU (Linux) tools.
release_changelog() {
  local old="## [Unreleased]"
  awk -v old="$old" -v new="## [v$VERSION] - $(date +%Y-%m-%d)" '
    !done && index($0, old) == 1 { print new substr($0, length(old) + 1); done = 1; next }
    { print }
  ' CHANGELOG.md >CHANGELOG.md.tmp
  mv CHANGELOG.md.tmp CHANGELOG.md
}

# 1. The release commit and tag.
versions set-ini "$VERSION"
release_changelog
git add platformio.ini CHANGELOG.md
git commit -m "chore: release v$VERSION"
git tag -a "v$VERSION" -m "CrossRose v$VERSION"

# 2. Start the next version.
versions set-ini "$NEXT"
printf '## [Unreleased]\n\n' | cat - CHANGELOG.md >CHANGELOG.md.tmp
mv CHANGELOG.md.tmp CHANGELOG.md
git add platformio.ini CHANGELOG.md
git commit -m "chore: start v$NEXT"

echo
echo "Ready. Check with 'git log --oneline -3', then push:"
echo "  git push --atomic origin $BRANCH v$VERSION"
echo "(--atomic = the branch and the tag go up together, or neither does.)"
echo "If GitHub refuses the push: git tag -d v$VERSION && git reset --hard origin/$BRANCH, then start again."
