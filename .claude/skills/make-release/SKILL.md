---
name: make-release
description: Use when cutting a new TinyUSB release — version bump, regenerated files, the per-release changelog, and validation before the maintainer commits and tags.
---

# Cut a TinyUSB Release

**Don't commit or tag during prep** — leave changes unstaged for the maintainer (step 4). Work in a worktree. Agree the version `X.Y.Z` with the maintainer first (their call, not derivable from the diff).

## 1. Bump + regenerate

```bash
R=.claude/skills/make-release/scripts/release.py
$R bump X.Y.Z                                      # tusb_option.h, repository.yml, library.json, sonar-project.properties
.claude/skills/build-doc/scripts/gen_doc.py        # docs/reference/{boards,dependencies}.rst, hil_boards.md
.claude/skills/build-doc/scripts/gen_presets.py    # hw/bsp/BoardPresets.json + per-example CMakePresets.json
. "$IDF_PATH/export.sh" && python3 tools/family_json.py refresh   # hw/bsp/family.json: every board re-observed
```
`refresh` configures every cmake board once, Espressif included, so it needs every toolchain and every dependency at its pin (`tools/get_deps.py all`) and the ESP-IDF environment sourced; it exits 1 naming each board it could not observe, and a row it could not observe keeps its old value. Nothing between releases re-checks rows against `src/common/tusb_mcu.h` or `hw/bsp/family_support.cmake` edits; this step is where they catch up.
`bump` refuses a version that is not `X.Y.Z` or equals the current one, and refuses (writing nothing) when a file's version line no longer matches its pattern — fix the file or the pattern, never hand-edit around it. The regenerated files change only if boards, deps or the HIL rosters did (see the build-doc skill for what a diff there means).

## 2. Changelog — `docs/changelog/` (the hard part)

New file `docs/changelog/X.Y.Z.md`, listed **first** in `docs/changelog/index.rst`. The PR set comes from **commit reachability, not merge date** (a date query wrongly pulls in the prior release's changelog PR at the boundary):

```bash
$R prs --prev 0.20.0 [--head master] > /tmp/prs.txt   # '#N<TAB>title<TAB>[labels]', one PR per line
```
It refuses when `--prev` is not an ancestor of the head, and when a first-parent commit is not a merged PR (a direct push: decide what to do with it, do not silence the guard). A PR merged into a *feature branch* folds into its parent (won't appear alone) — reflect its final state in the parent's bullet.

**Curate** into the prior file's exact Markdown (MyST) style:
- Title = version (`# X.Y.Z`), then italic date (ask if unknown). Add to top of `index.rst`.
- Section order: **General** (New MCUs and Boards / Code Quality and Build / Documentation) → **API Changes** → **Device Stack** (per class) → **Host Stack** → **Controller Driver (DCD & HCD)** (per driver) → **Testing** → **Contributors**. Sections are `##`; each class/driver group is a `###` sub-heading, not a bullet.
- Single backticks for symbols; group related PRs into one bullet (don't dump). `Port *`/driver labels help bucket DCD/HCD.
- **Contributors**: unique non-bot PR authors, alphabetical (the only contributor credit — no separate page): `$R contributors --prev 0.20.0` prints the line; drop any CI/service account it did not recognise.

## 3. Validate (leave unstaged)

```bash
pre-commit run --all-files                         # every file step 1 regenerated, unit tests included
.claude/skills/build-doc/scripts/build_doc.py -c   # docs build clean, warnings fail it (see build-doc skill)
python3 .claude/skills/build/scripts/check_build.py --board stm32f407disco -e device/cdc_msc  # smoke build, a release changes no firmware
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
