# Follow-up: move rtt.py out of tinyusb for good

Deferred from the 2026-09-15 migration of the `rtt` skill to agentrc. The skill
(text, board notes, CLI and tests) now lives in agentrc; `tools/rtt.py` stays
here as a byte-identical copy because the HIL harness imports its classes.

## What exists

- `test/hil/helper/hil_util.py` loads `tools/rtt.py` by path at import time and
  re-exports `JlinkRtt`, `OpenocdRtt`, `RttError`, `strip_banner`,
  `RTT_BANNER_RE`; `hil_test.py` opens `JlinkRtt` as the console of a board with
  `"logger": "rtt"` (only `ea4088_quickstart` in `tinyusb.json`).
- `tools/ci_select.py` imports `hil_util` on GitHub-hosted ubuntu-latest runners
  (build.yml path-selection jobs), so the file must be present there today.
- `hil_ci.sh` stages `tools/rtt.py` next to `test/hil`; build.yml's path filter
  and the pre-commit `hil-test` pattern name the file; `test_hil_rtt.py` asserts
  the staging line and keeps the harness-side contracts only.
- agentrc `skills/rtt/scripts/rtt.py` is identical; `tests/test_rtt.py` there
  holds the class and CLI tests. Two copies drift unless one goes.

## Remaining work

1. `hil_util`: load the module lazily, inside the console factory and
   `strip_banner`, from `HIL_RTT_PY` (default
   `~/.claude/skills/rtt/scripts/rtt.py`), registered in `sys.modules` so
   `RttError` still pickles across the fork pool; the ImportError names the
   resolved path, the variable and the install command. `ci_select` then never
   touches it.
2. Delete `tools/rtt.py`; drop the `hil_ci.sh` staging line, the build.yml path
   filter entry, the pre-commit pattern and the staging assertion.
3. `test_hil_rtt.py`: loader tests against a stub module (default path,
   override, missing-install diagnostic, console-factory wiring); nothing that
   needs the real implementation, so pre-commit stays agentrc-free.
4. Rigs: agentrc installed on ci.lan (already has a checkout) and on the tusb
   rig as user `tusb` from a persistent clone (`install.py install --skill`),
   after telling hifiphile, whose local HIL runs then need it. Then a real
   `hil_test.py` run on `ea4088_quickstart` on each rig before this lands.

## Trade-off (raised in review)

A tinyusb revision stops pinning its console implementation: what runs on a
rig is whatever `~/.claude/skills/rtt` holds that day, so a harness regression
cannot be bisected in this repository alone. Vendoring a pinned copy synced
from agentrc is the alternative if that matters more than one source of truth.
