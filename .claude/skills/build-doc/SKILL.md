---
name: build-doc
description: Use when building, previewing, or testing the TinyUSB Sphinx docs locally (docs/ → HTML), chasing Sphinx warnings, understanding how example READMEs get into the docs, or regenerating the auto-generated reference files after adding a board, a dependency, or a HIL rig board (boards.rst, dependencies.rst, hil_boards.md, BoardPresets.json, CMakePresets.json).
---

# Build TinyUSB Docs

## Build & preview

```bash
pip install -r docs/requirements.txt          # one-time
.claude/skills/build-doc/scripts/build_doc.py -o   # build docs/_build/ and open it; warnings fail it
```

`build_doc.py` wraps `sphinx-build -W`: `-c` clean, `-o` open, `--no-strict` for a preview that tolerates warnings. A warning names a doc bug (broken ref, page missing from a toctree), never noise, so fix it rather than opt out.

- Pages can be `.rst` or `.md` (MyST). Example `README.md`s under `examples/{device,host,dual}/*/` are **auto-collected** at build time into `docs/examples/` (per-group `index` pages, git-ignored) — add/rename an example and just rebuild; edit the source README, never the generated copies.

## Regenerate after adding a board or dependency

`docs/reference/{boards,dependencies}.rst`, `docs/reference/hil_boards.md` and the preset JSONs are **generated** — don't hand-edit. Scripts run from anywhere:

| Added | Run |
|---|---|
| Board (`hw/bsp/FAMILY/boards/`) | `.claude/skills/build-doc/scripts/gen_doc.py` + `.claude/skills/build-doc/scripts/gen_presets.py` |
| Dependency (edited `tools/get_deps.py`) | `.claude/skills/build-doc/scripts/gen_doc.py` |
| HIL board roster (`test/hil/tinyusb.json`, `hfp.json`) | `.claude/skills/build-doc/scripts/gen_doc.py` |

- `gen_doc.py` → `dependencies.rst` (from `tools/get_deps.py`'s table; `get_deps.py --gen-doc` writes it alone), `boards.rst` and `hil_boards.md` (the roster partial included by `hardware-in-the-loop.md`). The `gen-doc` pre-commit hook runs it whenever a `board.h`, `family.c`, `get_deps.py` or roster JSON is committed, so a regen you forgot shows up as "files were modified by this hook": stage them and commit again.
- `boards.rst` needs a `/* metadata: manufacturer: … */` block in the family's `family.c` and one with `name:`/`url:` (`note:` optional) in the board's `board.h`. A board or family without a block is left out and listed on stderr (exit 0); a block missing a field leaves that cell empty. After adding a board, check its row in the diff.
- `gen_doc.py` rewrites all three files whichever one you came for, so the diff also shows drift that predates your change. Commit the rows your change produced with it; report unrelated drift and leave it to its own commit — neither revert it nor bundle it.
- `gen_presets.py` → `hw/bsp/BoardPresets.json` + per-example `CMakePresets.json`.

Then rebuild the docs and `git diff` the regenerated files.
