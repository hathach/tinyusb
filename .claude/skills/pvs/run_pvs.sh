#!/usr/bin/env bash
# Run PVS-Studio static analysis on TinyUSB for a given BOARD.
#
# Implements the build + analyze flow behind CLAUDE.md "Static Analysis (PVS-Studio)":
#   - build all examples for BOARD with compile_commands.json exported
#   - run pvs-studio-analyzer against that compile DB using .PVS-Studio/.pvsconfig
#   - convert the log to human-readable (errorfile) and SARIF output
#
# Usage:
#   .claude/skills/pvs/run_pvs.sh <BOARD> [extra pvs-studio-analyzer args...]
#
# Examples:
#   .claude/skills/pvs/run_pvs.sh raspberry_pi_pico
#   # Scope to specific files via a plaintext list (one path per line):
#   printf 'src/tusb.c\nsrc/class/cdc/cdc_device.c\n' > /tmp/files.txt
#   .claude/skills/pvs/run_pvs.sh stm32f407disco -S /tmp/files.txt
set -euo pipefail

BOARD="${1:-}"
if [ -z "$BOARD" ]; then
  echo "Usage: $0 <BOARD> [extra pvs-studio-analyzer args...]" >&2
  echo "Example: $0 raspberry_pi_pico" >&2
  exit 2
fi
shift

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="examples/cmake-build-${BOARD}"
COMPILE_DB="${BUILD_DIR}/compile_commands.json"
REPORT_LOG="pvs-${BOARD}.log"
SARIF_OUT="pvs-${BOARD}.sarif"
JOBS="$(nproc 2>/dev/null || echo 4)"

# --- License ---------------------------------------------------------------
# Use an existing license file, else materialize one from PVS_STUDIO_CREDENTIALS
# (format: "<name> <key>", as supplied by the CI secret of the same name).
if ! pvs-studio-analyzer lic-info >/dev/null 2>&1; then
  if [ -n "${PVS_STUDIO_CREDENTIALS:-}" ]; then
    echo ">>> Registering PVS-Studio license from PVS_STUDIO_CREDENTIALS"
    # Split "<name> <key>" into exactly two fields, quoted, so a key containing
    # glob chars or extra spaces can't be word-split/expanded.
    read -r _pvs_name _pvs_key <<< "$PVS_STUDIO_CREDENTIALS"
    pvs-studio-analyzer credentials "$_pvs_name" "$_pvs_key"
  else
    echo "ERROR: no PVS-Studio license found and PVS_STUDIO_CREDENTIALS is unset." >&2
    exit 1
  fi
fi

# --- Build (exports compile_commands.json) ---------------------------------
echo ">>> Building all examples for ${BOARD} (this also fetches the compile DB)"
cmake examples -B "${BUILD_DIR}" -G Ninja \
  -DBOARD="${BOARD}" \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "${BUILD_DIR}"

if [ ! -f "${COMPILE_DB}" ]; then
  echo "ERROR: ${COMPILE_DB} was not produced by the build." >&2
  exit 1
fi

# --- Analyze ---------------------------------------------------------------
echo ">>> Running PVS-Studio analyzer (-j${JOBS})"
# Note: --dump-files scatters .PVS-Studio.i/.cfg dump files across the source
# tree (only useful for debugging false positives). It is omitted here to keep
# the working tree clean; add it back via "$@" if needed.
pvs-studio-analyzer analyze \
  -f "${COMPILE_DB}" \
  -R .PVS-Studio/.pvsconfig \
  -o "${REPORT_LOG}" -j"${JOBS}" \
  --security-related-issues \
  --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser \
  "$@"

# --- Report ----------------------------------------------------------------
echo ">>> General-analysis + MISRA findings (errorfile):"
plog-converter -a GA:1,2 -t errorfile "${REPORT_LOG}" || true
plog-converter -t sarif -o "${SARIF_OUT}" "${REPORT_LOG}" >/dev/null

echo ">>> Done."
echo "    Raw log : ${REPORT_LOG}"
echo "    SARIF   : ${SARIF_OUT}"
