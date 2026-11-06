#!/usr/bin/env bash
# Make a CrossRose stable release.
#
# Usage: ./scripts/release.sh <version> <next-version>    (on main)
#        ./scripts/release.sh <version>                   (on a release/ branch)
#   ./scripts/release.sh 2026.12.0 2027.03.0
#
# Versions are year.month.fix: 2026.12.0 is the December 2026 release,
# 2026.12.1 is a bug-fix release for it.
#
# What it does (on your computer only, nothing is pushed):
#   1. Checks you are on main (or a release/ branch), with no uncommitted
#      changes, and the same as GitHub.
#   2. Renames "## [Unreleased]" in CHANGELOG.md to "## [v<version>] - <today>".
#   3. Sets [crossink] version in platformio.ini to <version>.
#   4. Commits, and makes the tag v<version>.
#   5. A second commit sets the version to <next-version> and starts a new
#      "## [Unreleased]" section. Nightly builds are named after this next
#      target. On main it is required: without it, later nightlies would carry
#      the number of a version that is already out, and devices on a nightly
#      would be offered that older stable version as an "update".
#
# Then push the branch and the tag together. The tag starts the Release
# workflow on GitHub, which builds the firmware and makes a draft release.
set -euo pipefail

VERSION="${1:-}"
NEXT="${2:-}"
VERSION_RE='^[0-9]+\.[0-9]+\.[0-9]+$'

usage() {
  echo "Usage: $0 <version> <next-version>   (example: $0 2026.12.0 2027.03.0)" >&2
  exit 1
}
[[ "$VERSION" =~ $VERSION_RE ]] || usage
[[ -z "$NEXT" || "$NEXT" =~ $VERSION_RE ]] || usage

cd "$(git rev-parse --show-toplevel)"

fail() {
  echo "Stopped: $1" >&2
  exit 1
}

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ "$BRANCH" == "main" || "$BRANCH" == release/* ]] ||
  fail "you are on '$BRANCH'. Releases are made from main (or a release/ branch)."
[[ -z "$(git status --porcelain --untracked-files=no)" ]] ||
  fail "you have uncommitted changes. Commit or stash them first."
git fetch --quiet origin
[[ "$(git rev-parse HEAD)" == "$(git rev-parse "origin/$BRANCH")" ]] ||
  fail "your $BRANCH is not the same as GitHub's. Run 'git pull' (or push) first."
! git rev-parse -q --verify "refs/tags/v$VERSION" >/dev/null ||
  fail "the tag v$VERSION already exists."
grep -q '^## \[Unreleased\]' CHANGELOG.md ||
  fail "CHANGELOG.md has no '## [Unreleased]' section."

# Prints the higher of two versions (compares each number, so 2026.12 > 2026.3).
higher_version() {
  printf '%s\n%s\n' "$1" "$2" | sort -t. -k1,1n -k2,2n -k3,3n | tail -n1
}
if [[ "$BRANCH" == "main" && -z "$NEXT" ]]; then
  fail "on main, give the next target too, for example: $0 $VERSION 2027.03.0"
fi
if [[ -n "$NEXT" ]]; then
  [[ "$NEXT" != "$VERSION" && "$(higher_version "$VERSION" "$NEXT")" == "$NEXT" ]] ||
    fail "the next version ($NEXT) must be higher than $VERSION."
fi

set_version() {
  sed -i '' "/^\\[crossink\\]/,/^\\[/ s/^version = .*/version = $1/" platformio.ini
}

# 1. The release commit and tag.
set_version "$VERSION"
sed -i '' "s/^## \\[Unreleased\\]/## [v$VERSION] - $(date +%Y-%m-%d)/" CHANGELOG.md
git add platformio.ini CHANGELOG.md
git commit -m "chore: release v$VERSION"
git tag -a "v$VERSION" -m "CrossRose v$VERSION"

# 2. Start the next version.
if [[ -n "$NEXT" ]]; then
  set_version "$NEXT"
  printf '## [Unreleased]\n\n' | cat - CHANGELOG.md > CHANGELOG.md.tmp
  mv CHANGELOG.md.tmp CHANGELOG.md
  git add platformio.ini CHANGELOG.md
  git commit -m "chore: start v$NEXT"
fi

echo
echo "Ready. Check with 'git log --oneline -3', then push:"
echo "  git push --atomic origin $BRANCH v$VERSION"
echo "(--atomic = the branch and the tag go up together, or neither does.)"
echo "If GitHub refuses the push: git tag -d v$VERSION && git reset --hard origin/$BRANCH, then start again."
