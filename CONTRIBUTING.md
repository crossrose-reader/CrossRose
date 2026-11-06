# Contributing to CrossRose

Thank you for helping! CrossRose takes pull requests from forks.
You do not need any special access: anyone with a GitHub account can do this.

## The short version

1. Fork `crossrose-reader/CrossRose` (the **Fork** button on GitHub).
2. In your fork, make a branch for your change.
3. Push the branch to your fork.
4. Open a pull request into **`crossrose-reader/CrossRose`**, branch **`main`**.

## Step by step

```sh
# 1. Get your fork (change YOUR-NAME), with its submodules
git clone --recursive https://github.com/YOUR-NAME/CrossRose.git
cd CrossRose

# 2. Remember where the main project is, so you can get new changes later
git remote add upstream https://github.com/crossrose-reader/CrossRose.git

# 3. Start a branch from the newest main
git fetch upstream
git switch -c fix/short-name upstream/main

# 4. Change code, build, and test on your device
pio run -e default                  # build for Xteink X3/X4
pio run -e default --target upload  # flash a connected device

# 5. Format the code, then commit
./bin/clang-format-fix
git commit -am "fix: short summary of the change"

# 6. Push to YOUR fork, then open the pull request on GitHub
git push -u origin fix/short-name
```

**Check the target before you press "Create pull request".**
GitHub shows `base repository:` at the top of the page. It must say
`crossrose-reader/CrossRose`, branch `main`. CrossRose is itself a fork, so
GitHub sometimes picks `uxjulia/CrossInk` instead.

## Branch and commit names

- Branch names start with a type: `feat/`, `fix/`, `docs/`, `refactor/`, `test/`, or `chore/`.
- Commit messages look like `<type>: <short summary>`, for example `fix: keep bookmarks after sleep`.

## Before you open the pull request

- [ ] It builds: `pio run -e default` (and `-e x4-pro` if you touched ESP32-S3 code).
- [ ] The code is formatted: `./bin/clang-format-fix`.
- [ ] You tested it on a real device, or you say in the pull request that you could not.
- [ ] User-visible changes have a line in `CHANGELOG.md` under `## [Unreleased]`.
- [ ] New text on the screen uses `tr(STR_...)` so it can be translated.

When you open the pull request, the checks (CI) run on it. A maintainer must
approve the checks before they start, and this can happen more than once on the
same pull request. This protects the project. It does not mean something is wrong.

## What fits in CrossRose

Read [SCOPE.md](./SCOPE.md) first. CrossRose is a reader: changes that make
reading better are welcome. Apps, games, and background internet features are not.
For a big feature, please open a discussion first, so nobody wastes work.

## Releases

- **Nightly**: built every night from `main`. Your change is in the next nightly after it is merged.
- **Stable**: numbered versions like `v0.1.0`. The 0.x versions lead up to 1.0.0,
  the first big release. Each planned version has a milestone on GitHub that shows
  what is planned for it.
