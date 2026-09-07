---
name: build-doc
description: Use when building, previewing, or testing the TinyUSB Sphinx docs locally (docs/ → HTML), chasing Sphinx warnings, understanding how example READMEs get into the docs, or regenerating the auto-generated reference files after adding a board, a dependency, or a HIL rig board (boards.rst, dependencies.rst, hil_boards.md, BoardPresets.json, CMakePresets.json).
---

# Build TinyUSB Docs

## Build & preview

```bash
pip install -r docs/requirements.txt    # one-time
python3 tools/build_doc.py -o            # build docs/_build/ and open it
```

`tools/build_doc.py` wraps `sphinx-build`: `-c` clean, `-W` fail on warnings, `-o` open. Raw form: `sphinx-build -b html docs docs/_build`.

- Pages can be `.rst` or `.md` (MyST). Example `README.md`s under `examples/{device,host,dual}/*/` are **auto-collected** at build time into `docs/examples/` (per-group `index` pages, git-ignored) — add/rename an example and just rebuild; edit the source README, never the generated copies.
- Watch the output for `WARNING:` (broken refs, missing toctree entries).

## Regenerate after adding a board or dependency

Run from the repo root; `docs/reference/*.rst`, `docs/reference/hil_boards.md` and the preset JSONs are **generated** — don't hand-edit.

| Added | Run |
|---|---|
| Board (`hw/bsp/FAMILY/boards/`) | `python3 tools/gen_doc.py` + `python3 tools/gen_presets.py` |
| Dependency (edited `tools/get_deps.py`) | `python3 tools/gen_doc.py` |
| HIL board roster (`test/hil/tinyusb.json`, `hfp.json`) | `python3 tools/gen_doc.py` |

- `gen_doc.py` → `docs/reference/boards.rst` + `dependencies.rst` + `hil_boards.md` (the roster partial included by `hardware-in-the-loop.md`). Needs `pandas` + `tabulate` (not in `requirements.txt`) — `pip install pandas tabulate` if it errors.
- `gen_doc.py` rewrites all three files whichever one you came for; revert any unrelated churn in `boards.rst`/`dependencies.rst` before committing.
- `gen_presets.py` → `hw/bsp/BoardPresets.json` + per-example `CMakePresets.json`.

Then rebuild and `git diff` the regenerated files; commit them with the board/dep change.
