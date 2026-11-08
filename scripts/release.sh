#!/usr/bin/env bash
# Make a CrossRose stable release.
#
# Usage: ./scripts/release.sh <version> <next-version>
#        ./scripts/release.sh branch <MAJOR.MINOR>
#   ./scripts/release.sh 1.0.0 1.1.0    on main
#   ./scripts/release.sh branch 1.0     makes release/1.0, for fixes to 1.0.x
#   ./scripts/release.sh 1.0.1 1.0.2    on release/1.0 (a fix for an older version)
#
# Versions are MAJOR.MINOR.PATCH ("semantic versioning"):
#   1.1.0 -> 1.2.0  new features (MINOR goes up)
#   1.1.0 -> 1.1.1  only bug fixes (PATCH goes up)
#   1.0.0           the first stable release
#
# What a release does (on your computer only, nothing is pushed):
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
#        <version> must be higher than every 0.1.x release, and lower than
#        the version main is working toward.
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
# "branch 1.0" makes release/1.0 from the newest v1.0.x tag on GitHub, and
# commits the start of the next fix version (1.0.<x+1>, with a new
# "## [Unreleased]" section). Push it, commit the fixes, then release.
#
# Works with bash 3.2 (macOS) and newer, and with BSD or GNU tools
# (macOS, Linux, WSL, Git Bash). .gitattributes keeps *.sh files LF.
set -euo pipefail

usage() {
  echo "Usage: $0 <version> <next-version>   (example: $0 1.0.0 1.1.0)" >&2
  echo "       $0 branch <MAJOR.MINOR>        (example: $0 branch 1.0)" >&2
  exit 1
}
[[ $# -eq 2 ]] || usage

cd "$(git rev-parse --show-toplevel)"

# All version rules (format, order, reading and writing platformio.ini) live
# in one helper that the GitHub workflows use too.
versions() {
  bash scripts/crossrose_version.sh "$@"
}

fail() {
  echo "Stopped: $1" >&2
  exit 1
}

check_clean() {
  [[ -z "$(git status --porcelain --untracked-files=no)" ]] ||
    fail "you have uncommitted changes. Commit or stash them first."
}

# The releases that exist are the v* tags on GitHub. Ask GitHub directly, so
# tags that are only on this computer (for example CrossInk's, if you fetched
# tags from the upstream remote) do not count.
read_remote_tags() {
  REMOTE_TAGS="$(git ls-remote --tags --refs origin)" ||
    fail "could not read the tags on GitHub."
}

# Prints the newest release on GitHub, or the newest of one MAJOR.MINOR line.
newest_release() {
  printf '%s\n' "$REMOTE_TAGS" | cut -f2 | versions highest "$@"
}

# A fix release for an older line must stay below the version main is working
# toward. Otherwise it would take main's next version: main's nightly would
# stop with "already released", and builds of main would be offered the fix,
# which lacks main's changes, as an "update".
check_below_main() {
  local fix="$1" main_ini main_target major rest minor
  main_ini="$(git show origin/main:platformio.ini)" ||
    fail "could not read platformio.ini on GitHub's main."
  main_target="$(printf '%s\n' "$main_ini" | versions ini -)" ||
    fail "platformio.ini on GitHub's main has no valid [crossink] version."
  if ! versions gt "$main_target" "$fix"; then
    major="${main_target%%.*}"
    rest="${main_target#*.}"
    minor="${rest%%.*}"
    fail "main is working toward $main_target, so $fix would take main's next version. First raise [crossink] version on main (for example to $major.$((minor + 1)).0) in a commit of its own, and push it."
  fi
}

# Starts the next version: sets platformio.ini and opens a new
# "## [Unreleased]" section at the top of CHANGELOG.md, then commits.
start_version() {
  versions set-ini "$1"
  printf '## [Unreleased]\n\n' | cat - CHANGELOG.md >CHANGELOG.md.tmp
  mv CHANGELOG.md.tmp CHANGELOG.md
  git add platformio.ini CHANGELOG.md
  git commit -m "chore: start v$1"
}

# ./scripts/release.sh branch <MAJOR.MINOR>
make_release_branch() {
  local line="$1" branch newest patch fix
  versions valid "$line.0" ||
    fail "'$line' is not a MAJOR.MINOR line like 0.1."
  branch="release/$line"
  check_clean
  git fetch --quiet origin
  ! git rev-parse -q --verify "refs/heads/$branch" >/dev/null ||
    fail "you already have a $branch branch."
  ! git rev-parse -q --verify "refs/remotes/origin/$branch" >/dev/null ||
    fail "$branch is already on GitHub. Get it with 'git checkout $branch'."
  read_remote_tags
  newest="$(newest_release "$line")"
  [[ -n "$newest" ]] ||
    fail "no $line.x release is on GitHub yet. Release $line.0 from main."
  patch="${newest##*.}"
  fix="$line.$((patch + 1))"
  check_below_main "$fix"

  # Use GitHub's tag. Stops if a local tag of that name points elsewhere.
  git fetch --quiet origin "refs/tags/v$newest:refs/tags/v$newest" ||
    fail "your tag v$newest is not the same as GitHub's. Delete it with 'git tag -d v$newest' and try again."
  git checkout --quiet -b "$branch" "v$newest"
  start_version "$fix"

  echo
  echo "Ready: you are on $branch, made from v$newest. It works toward $fix."
  echo "Push it:  git push -u origin $branch"
  echo "Then commit the fixes (each with a line under '## [Unreleased]' in CHANGELOG.md),"
  echo "push them, and release:  ./scripts/release.sh $fix $line.$((patch + 2))"
}

if [[ "$1" == "branch" ]]; then
  make_release_branch "$2"
  exit 0
fi

VERSION="$1"
NEXT="$2"
if ! versions valid "$VERSION" || ! versions valid "$NEXT"; then
  usage
fi

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
check_clean
git fetch --quiet origin
[[ "$(git rev-parse HEAD)" == "$(git rev-parse "origin/$BRANCH")" ]] ||
  fail "your $BRANCH is not the same as GitHub's. Run 'git pull' (or push) first."
! git rev-parse -q --verify "refs/tags/v$VERSION" >/dev/null ||
  fail "the tag v$VERSION already exists."
if ! grep -q '^## \[Unreleased\]' CHANGELOG.md; then
  if [[ -n "$LINE" ]]; then
    fail "CHANGELOG.md has no '## [Unreleased]' section. Make release branches with './scripts/release.sh branch $LINE', which adds it."
  fi
  fail "CHANGELOG.md has no '## [Unreleased]' section."
fi
versions gt "$NEXT" "$VERSION" ||
  fail "the next version ($NEXT) must be higher than $VERSION."

# The version this branch is working toward. Stops if it is missing or bad.
CURRENT="$(versions ini)"

# A release/0.1 branch makes 0.1.x fixes and nothing else. Otherwise a typo
# (0.2.1 for 0.1.2) would tag the old 0.1 code as the newest version, and
# devices on 0.2.0 would be offered it as an "update".
if [[ -n "$LINE" ]]; then
  [[ "${CURRENT%.*}" == "$LINE" ]] ||
    fail "platformio.ini on $BRANCH says $CURRENT, which is not a $LINE.x version. Make release branches with './scripts/release.sh branch $LINE'."
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

read_remote_tags
if [[ -z "$LINE" ]]; then
  NEWEST="$(newest_release)"
  WHICH="the newest release"
else
  NEWEST="$(newest_release "$LINE")"
  WHICH="the newest $LINE.x release"
  # Release branches are for fixes to a line that is already out, and must
  # not take the version main is working toward.
  [[ -n "$NEWEST" ]] ||
    fail "$BRANCH is for fixes to $LINE.x, but no $LINE.x release is on GitHub yet. Release $LINE.0 from main."
  check_below_main "$VERSION"
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
start_version "$NEXT"

echo
echo "Ready. Check with 'git log --oneline -3', then push:"
echo "  git push --atomic origin $BRANCH v$VERSION"
echo "(--atomic = the branch and the tag go up together, or neither does.)"
echo "If GitHub refuses the push: git tag -d v$VERSION && git reset --hard origin/$BRANCH, then start again."
