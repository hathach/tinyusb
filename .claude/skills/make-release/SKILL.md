---
name: make-release
description: Use when cutting a new TinyUSB release — version bump, regenerated files, the per-release changelog, and validation before the maintainer commits and tags.
---

# Cut a TinyUSB Release

**Don't commit or tag during prep** — leave changes unstaged for the maintainer (step 4). Work in a worktree. Agree the version `X.Y.Z` with the maintainer first (their call, not derivable from the diff).

## 1. Bump + regenerate

```bash
# set version = 'X.Y.Z' in tools/make_release.py, then FROM REPO ROOT:
python3 tools/make_release.py
```
Refreshes `tusb_option.h`, `repository.yml`, `library.json`, `sonar-project.properties`, and (via gen_doc/gen_presets) `docs/reference/{boards,dependencies}.rst` + preset JSONs (presets/docs change only if boards/deps changed).

Gotchas: `gen_doc` needs `pandas`+`tabulate` (not in requirements) → `pip install pandas tabulate`; `boards.rst` lands with no trailing newline → let pre-commit fix it (step 3).

## 2. Changelog — `docs/changelog/` (the hard part)

New file `docs/changelog/X.Y.Z.rst`, listed **first** in `docs/changelog/index.rst`. Get the PR set by **commit reachability, not merge date** (a date query wrongly pulls in the prior release's changelog PR at the boundary):

```bash
PREV=0.20.0
git merge-base --is-ancestor $PREV HEAD && echo OK   # else stop: range invalid
git log --first-parent $PREV..HEAD --pretty=%s > /tmp/fp.txt
{ sed -nE 's/^Merge pull request #([0-9]+).*/\1/p' /tmp/fp.txt   # merge-button
  sed -nE 's/.*\(#([0-9]+)\)$/\1/p'                /tmp/fp.txt ; } | sort -un > /tmp/prs.txt   # squash
grep -vE '^Merge pull request #[0-9]+|\(#[0-9]+\)$' /tmp/fp.txt   # guard: must be empty (direct pushes)
xargs -P8 -I{} gh pr view {} --json number,title,labels \
  --jq '"#\(.number)\t\(.title)\t[\(.labels|map(.name)|join(","))]"' < /tmp/prs.txt   # categorize
```
`--first-parent` skips dev-merges; the two `sed`s catch merge-button + squash. A PR merged into a *feature branch* folds into its parent (won't appear alone) — reflect its final state in the parent's bullet.

**Curate** into the prior file's exact RST style:
- Title = version (`======` underline), then italic date (ask if unknown). Add to top of `index.rst`.
- Section order: **General** (New MCUs and Boards / Code Quality and Build / Documentation) → **API Changes** → **Device Stack** (per class) → **Host Stack** → **Controller Driver (DCD & HCD)** (per driver) → **Testing** → **Contributors**. Each class/driver group is a ``^^^`` sub-heading, not a bullet.
- Double-backticks for symbols; group related PRs into one bullet (don't dump). `Port *`/driver labels help bucket DCD/HCD.
- **Contributors**: unique non-bot PR authors, alphabetical (the only contributor credit — no separate page):
  ```bash
  xargs -P8 -I{} gh pr view {} --json author --jq '.author.login' < /tmp/prs.txt \
    | grep -viE '\[bot\]$|^(copilot|claude|dependabot|github-actions)$' | sort -uf | sed 's/^/@/' | paste -sd, - | sed 's/,/, /g'   # drop any CI/service accounts
  ```

## 3. Validate (leave unstaged)

```bash
pre-commit run --files docs/changelog/X.Y.Z.rst docs/changelog/index.rst \
  docs/reference/boards.rst docs/reference/dependencies.rst \
  library.json repository.yml sonar-project.properties src/tusb_option.h tools/make_release.py
python3 tools/build_doc.py -c            # docs build clean (see build-doc skill)
( cd test/unit-test && ceedling test:all )
( cd examples/device/cdc_msc && rm -rf build && mkdir build && cd build && \
  cmake -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .. && cmake --build . )
git diff --stat -- ':!.idea'             # .idea/* is IDE noise
```
Confirm the version matches across `tusb_option.h` / `library.json` / `repository.yml` / `sonar-project.properties`.

## 4. Finalize (maintainer)

```bash
git add -A -- ':!.idea' && git commit -m "Bump version to X.Y.Z"
git tag -a X.Y.Z -m "Release X.Y.Z"      # tags are unprefixed (0.20.0, not v0.20.0)
git push origin <branch> X.Y.Z
```
Then create the GitHub release from the tag.

## 5. Code size (automatic)

On the release event, CI's `code-metrics` job diffs against the previous tag's `metrics.json` and uploads `metrics.json` + a compare to the release — **only if the previous release has a `metrics.json` asset**. Confirm both appeared.
