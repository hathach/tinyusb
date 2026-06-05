---
name: pvs
description: Use when running PVS-Studio static analysis (SAST + MISRA C:2023 / C++:2008) on TinyUSB for a given board. Builds the examples with an exported compile_commands.json, runs pvs-studio-analyzer against .PVS-Studio/.pvsconfig, and emits readable + SARIF output.
---

# PVS-Studio Static Analysis

`run_pvs.sh <BOARD>` builds all examples for the board (with
`compile_commands.json` exported), runs `pvs-studio-analyzer` against
`.PVS-Studio/.pvsconfig`, then writes `pvs-<board>.log` and `pvs-<board>.sarif`
plus printed errorfile findings. Needs a license file or `$PVS_STUDIO_CREDENTIALS`.

```bash
.claude/skills/pvs/run_pvs.sh raspberry_pi_pico       # whole project for a board

# Scope to specific files — -S takes a plaintext list (one path per line),
# NOT a source file directly. Extra args pass through to the analyzer.
printf 'src/tusb.c\nsrc/class/cdc/cdc_device.c\n' > /tmp/files.txt
.claude/skills/pvs/run_pvs.sh stm32f407disco -S /tmp/files.txt
```

## Notes

- **Board:** `raspberry_pi_pico` mirrors CI (needs Pico SDK deps);
  `stm32f407disco` is fastest (no external SDK). Any board works once its deps
  are fetched (`python3 tools/get_deps.py -b <board>`).
- `.pvsconfig` already excludes vendored code and suppresses accepted MISRA
  deviations — don't re-add those on the command line. Surviving `src/` findings
  are genuinely new.
- Timing is dominated by the build (deps may push it past a minute); the analysis
  pass is ~10-30 s. Use a timeout ≥ 10 min when deps must be fetched first.
- `--dump-files` is intentionally omitted — it scatters `.PVS-Studio.i/.cfg`
  dumps across the tree (FP-debugging only); add it back via the passthrough args.

After a run, summarize findings by rule/severity from the errorfile output and
point the user at `pvs-<board>.sarif`.
