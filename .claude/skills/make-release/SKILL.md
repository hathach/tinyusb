---
name: make-release
description: Use when cutting a new TinyUSB release — bumping the version, running tools/make_release.py, writing the per-release docs/info/changelog/ entry from the PRs merged since the last tag, and validating before the maintainer commits and tags. Covers the changelog PR-boundary reconciliation and the regenerated-file gotchas.
---

# Cut a TinyUSB Release

**Do NOT commit or tag** during prep — leave every change unstaged for maintainer review; the maintainer finalizes (commit, tag, release) in step 4. Work in a worktree. Agree the target version `X.Y.Z` with the maintainer before starting (it is their call, not derivable from the diff).

## 1. Bump version + regenerate files

```bash
# edit tools/make_release.py: version = 'X.Y.Z'
python3 tools/make_release.py        # MUST run from repo root (uses relative paths)
```

Refreshes `src/tusb_option.h`, `repository.yml`, `library.json`, `sonar-project.properties`, and (via `gen_doc`/`gen_presets`) `docs/reference/{boards,dependencies}.rst`, `hw/bsp/BoardPresets.json`, per-example `CMakePresets.json`. The presets/docs only change if boards/deps changed since the last release.

Gotchas:
- `gen_doc` imports `pandas` + `tabulate` at line 3 and crashes before writing if they're missing (not in `docs/requirements.txt`) → `pip install pandas tabulate`. See the **build-doc** skill.
- The `repository.yml` regex inserts a literal `\r\n`, leaving one CRLF line → `sed -i 's/\r$//' repository.yml`.
- `boards.rst` is written with no trailing newline → pre-commit's `end-of-file-fixer` adds it; run pre-commit on the regenerated files (step 3).

## 2. Changelog — `docs/info/changelog/` (the hard part)

Each release is its own file: create `docs/info/changelog/X.Y.Z.rst` and add it as the **first** entry in the `docs/info/changelog/index.rst` toctree (newest first). Determine the PR set by **git commit reachability, not by merge date** — a date query (`gh ... --search merged:>=DATE`) both includes the *previous* release's own changelog PR (its `mergedAt` shares the tag day) and is awkward at the boundary; `--merges | grep "Merge pull request"` silently drops squash-merged PRs.

```bash
PREV=0.20.0   # last release tag
git merge-base --is-ancestor $PREV HEAD && echo OK   # else range math invalid — stop
git log --first-parent $PREV..HEAD --pretty=%s > /tmp/fp.txt
{ sed -nE 's/^Merge pull request #([0-9]+).*/\1/p' /tmp/fp.txt
  sed -nE 's/.*\(#([0-9]+)\)$/\1/p'                /tmp/fp.txt ; } | sort -un > /tmp/prs.txt
# guard: any mainline commit with NO PR ref (direct push / rebase-merge). Must be empty; investigate if not:
grep -vE '^Merge pull request #[0-9]+|\(#[0-9]+\)$' /tmp/fp.txt
# titles + labels to categorize (parallelize — 200+ sequential gh calls take minutes):
xargs -P8 -I{} gh pr view {} --json number,title,labels \
  --jq '"#\(.number)\t\(.title)\t[\(.labels|map(.name)|join(","))]"' < /tmp/prs.txt
```

Why this is correct: `$PREV..HEAD` excludes everything reachable from the last tag (nothing already shipped leaks in — including the prior release's own changelog PR, which the tag sits on); `--first-parent` skips `Merge branch … into <feature>` dev-merges that aren't PRs; the two `sed` patterns catch both merge-button (`Merge pull request #N` — the ` from <branch>` suffix is sometimes edited out) and squash (`… (#N)`) merges. The anchored patterns avoid grabbing stray `#numbers` from prose/issue refs.

A PR merged into a *feature branch* (rather than master) is folded into its parent top-level PR and won't appear on its own — intended (review follow-ups aren't double-listed), but make the parent's changelog bullet reflect the final merged state. This is also why a date-based `gh` query returns *more* numbers than this set: those extras are the prior changelog PR plus these sub-PRs.

Then **curate** into the existing sections, matching the prior release file's RST style exactly:
- The file's title is the version (`X.Y.Z` with `======` underline matching its length), then the italic date (the planned tag date — ask if unknown). Add the file to the top of `docs/info/changelog/index.rst`.
- Sections in order: **General** (New MCUs and Boards / Code Quality and Build / Documentation), **API Changes**, **Device Stack** (per class: Audio, CDC, HID, MIDI, MSC, MTP, Net, Video, …), **Host Stack**, **Controller Driver (DCD & HCD)** (per driver: DWC2, FSDEV, MUSB, RP2040, …), **Testing**, **Contributors**. Within a section, each driver/class group is a ``^^^``-underlined sub-heading (not a bullet).
- RST inline code (double backticks) for symbols. Group related PRs into one bullet — summarize, don't dump 200 lines. Driver/`Port *` PR labels help bucket DCD/HCD entries.
- **Contributors** section credits the release's PR authors (this is where contributor credit lives — there is no separate contributors page). List the unique non-bot author handles alphabetically:

  ```bash
  while read n; do gh pr view "$n" --json author --jq '.author.login'; done < /tmp/prs.txt \
    | grep -viE 'bot|copilot|claude' | sort -uf | sed 's/^/@/' | paste -sd', '
  ```

## 3. Validate (all unstaged)

```bash
pre-commit run --files docs/info/changelog/X.Y.Z.rst docs/info/changelog/index.rst \
  docs/reference/boards.rst docs/reference/dependencies.rst \
  library.json repository.yml sonar-project.properties src/tusb_option.h tools/make_release.py
python3 tools/build_doc.py -c                                 # docs build clean; new release page wired into the toctree (see build-doc skill)
cd test/unit-test && ceedling test:all                       # expect all pass
cd examples/device/cdc_msc && rm -rf build && mkdir build && cd build && \
  cmake -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .. && cmake --build .
git diff --stat -- ':!.idea'                                 # review; `.idea/*` is IDE noise, exclude it
```

Confirm version is consistent across `tusb_option.h` / `library.json` / `repository.yml` / `sonar-project.properties`, then leave everything unstaged for the maintainer.

## 4. Finalize (maintainer)

After reviewing the unstaged diff:

```bash
git commit -am "Bump version to X.Y.Z"
git tag -a vX.Y.Z -m "Release X.Y.Z"
git push origin <branch> vX.Y.Z
```

Then create a GitHub release from the `vX.Y.Z` tag.
