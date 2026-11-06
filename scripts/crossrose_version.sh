#!/usr/bin/env bash
# Read, check and compare CrossRose version numbers.
#
# One place for these rules, so scripts/release.sh and the Nightly and Release
# workflows can never disagree about them.
#
# A stable version is MAJOR.MINOR.PATCH, three whole numbers without leading
# zeros: 0.1.0, 0.10.2, 1.0.0. Tags add a "v" in front: v0.1.0.
#
#   crossrose_version.sh ini [file]
#       Print [crossink] version from platformio.ini (or <file>).
#       Spaces around "=" and Windows line endings are fine.
#       Stops with an error if it is missing or not a stable version.
#   crossrose_version.sh set-ini <version> [file]
#       Set [crossink] version in platformio.ini (or <file>).
#   crossrose_version.sh valid <version>
#       Succeed if <version> is a stable version.
#   crossrose_version.sh gt <a> <b>
#       Succeed if version <a> is higher than version <b> (0.10.0 > 0.9.0).
#   crossrose_version.sh highest [MAJOR.MINOR]
#       Read tag names on stdin (v0.1.0 or refs/tags/v0.1.0, one per line) and
#       print the highest stable version, without the "v". Prints nothing if
#       there is none. Other tags (nightly, v0.1.0-rc) are ignored.
#       With MAJOR.MINOR (like 0.1), only tags of that line count.
#
# Works with bash 3.2 (macOS) and newer, and with any POSIX awk (BSD awk on
# macOS, gawk or mawk on Linux). Git Bash on Windows works only if this file
# is checked out with LF line endings; with Windows (CRLF) endings bash stops
# at "set -o pipefail".
set -euo pipefail

VERSION_RE='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'
LINE_RE='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

# In GitHub Actions, "::error::" makes the message show up on the run page.
error() {
  if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
    echo "::error::$*" >&2
  else
    echo "$(basename "$0"): $*" >&2
  fi
}

usage() {
  echo "Usage: $(basename "$0") ini [file] | set-ini <version> [file] | valid <version>" >&2
  echo "       | gt <a> <b> | highest [MAJOR.MINOR]   (details at the top of $0)" >&2
  exit 2
}

is_valid() {
  [[ "$1" =~ $VERSION_RE ]]
}

# Succeeds if $1 > $2. Both must be valid versions.
is_higher() {
  local a b i
  IFS=. read -r -a a <<<"$1"
  IFS=. read -r -a b <<<"$2"
  for i in 0 1 2; do
    if ((a[i] > b[i])); then return 0; fi
    if ((a[i] < b[i])); then return 1; fi
  done
  return 1
}

# The [crossink] section starts at a "[crossink]" line and ends at the next
# "[...]" line. Inside it, "version" is the key before the first "=".
# Prints the trimmed value of the first version line; fails if there is none.
read_ini() {
  awk '
    { sub(/\r$/, "") }
    /^[ \t]*\[/ {
      name = $0
      sub(/^[ \t]*\[/, "", name)
      sub(/\][ \t]*$/, "", name)
      in_section = (name == "crossink")
      next
    }
    in_section {
      eq = index($0, "=")
      if (eq == 0) next
      key = substr($0, 1, eq - 1)
      sub(/^[ \t]+/, "", key)
      sub(/[ \t]+$/, "", key)
      if (tolower(key) != "version") next
      value = substr($0, eq + 1)
      sub(/^[ \t]+/, "", value)
      sub(/[ \t]+$/, "", value)
      print value
      found = 1
      exit
    }
    END { exit(found ? 0 : 1) }
  ' "$1"
}

# Rewrites the first version line of [crossink] as "version = <new>", keeping
# a Windows line ending if the line had one. Fails (and changes nothing) if
# there is no such line.
write_ini() {
  local new="$1" file="$2" tmp="$2.tmp"
  if ! awk -v new="$new" '
    {
      line = $0
      cr = sub(/\r$/, "", line)
    }
    line ~ /^[ \t]*\[/ {
      name = line
      sub(/^[ \t]*\[/, "", name)
      sub(/\][ \t]*$/, "", name)
      in_section = (name == "crossink")
    }
    in_section && !done && index(line, "=") > 0 {
      key = substr(line, 1, index(line, "=") - 1)
      sub(/^[ \t]+/, "", key)
      sub(/[ \t]+$/, "", key)
      if (tolower(key) == "version") {
        print "version = " new (cr ? "\r" : "")
        done = 1
        next
      }
    }
    { print }
    END { exit(done ? 0 : 1) }
  ' "$file" >"$tmp"; then
    rm -f "$tmp"
    return 1
  fi
  mv "$tmp" "$file"
}

cmd="${1:-}"
if [[ $# -gt 0 ]]; then shift; fi

case "$cmd" in
ini)
  file="${1:-platformio.ini}"
  if [[ ! -f "$file" ]]; then
    error "$file not found."
    exit 1
  fi
  if ! value="$(read_ini "$file")"; then
    error "$file has no 'version = ...' line in its [crossink] section."
    exit 1
  fi
  if ! is_valid "$value"; then
    error "$file: [crossink] version is '$value'. It must be a stable version like 0.1.0."
    exit 1
  fi
  echo "$value"
  ;;
set-ini)
  [[ $# -ge 1 ]] || usage
  new="$1"
  file="${2:-platformio.ini}"
  if ! is_valid "$new"; then
    error "'$new' is not a stable version like 0.1.0."
    exit 1
  fi
  if ! write_ini "$new" "$file"; then
    error "$file has no 'version = ...' line in its [crossink] section."
    exit 1
  fi
  ;;
valid)
  [[ $# -eq 1 ]] || usage
  is_valid "$1"
  ;;
gt)
  [[ $# -eq 2 ]] || usage
  for v in "$1" "$2"; do
    if ! is_valid "$v"; then
      error "'$v' is not a stable version like 0.1.0."
      exit 2
    fi
  done
  is_higher "$1" "$2"
  ;;
highest)
  line="${1:-}"
  if [[ -n "$line" && ! "$line" =~ $LINE_RE ]]; then
    error "'$line' is not a MAJOR.MINOR line like 0.1."
    exit 2
  fi
  best=""
  while IFS= read -r tag || [[ -n "$tag" ]]; do
    tag="${tag%$'\r'}"
    tag="${tag#refs/tags/}"
    [[ "$tag" == v* ]] || continue
    v="${tag#v}"
    is_valid "$v" || continue
    if [[ -n "$line" && "$v" != "$line".* ]]; then continue; fi
    if [[ -z "$best" ]] || is_higher "$v" "$best"; then best="$v"; fi
  done
  if [[ -n "$best" ]]; then echo "$best"; fi
  ;;
*)
  usage
  ;;
esac
