#!/usr/bin/env bash
# Run HIL test remotely on ci.lan
# Usage: test/hil/hil_ci.sh [-b BOARD] [-t TEST] [extra hil_test.py args...]
# Example:
#   test/hil/hil_ci.sh -b stm32f723disco
#   test/hil/hil_ci.sh -b stm32f723disco -t host/cdc_msc_hid -r 1
#
# Env overrides: REMOTE, REMOTE_DIR, CONFIG (path to HIL config json),
# ROOT_DIR (tinyusb checkout to test; defaults to the script's own checkout).

set -euo pipefail

REMOTE=${REMOTE:-ci.lan}
REMOTE_DIR=${REMOTE_DIR:-/tmp/tinyusb-hil}
ROOT_DIR=${ROOT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}
CONFIG=${CONFIG:-$ROOT_DIR/test/hil/tinyusb.json}

[[ -f "$ROOT_DIR/test/hil/hil_test.py" && -d "$ROOT_DIR/examples" ]] || {
  echo "error: $ROOT_DIR does not look like a tinyusb checkout" >&2
  exit 1
}

# Parse -b BOARD from arguments to know which build to copy
BOARD=""
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -b)
      [[ $# -ge 2 ]] || { echo "error: -b requires a BOARD argument" >&2; exit 1; }
      BOARD="$2"
      ARGS+=("$1" "$2")
      shift 2
      ;;
    *)
      ARGS+=("$1")
      shift
      ;;
  esac
done

# Setup remote directory. Use `bash -s` + heredoc so REMOTE_DIR (user-overridable)
# is passed as a positional parameter and never reinterpreted by the remote shell.
echo "==> Setting up remote $REMOTE:$REMOTE_DIR"
ssh "$REMOTE" bash -s -- "$REMOTE_DIR" <<'REMOTE'
set -e
rm -rf -- "$1"
mkdir -p -- "$1/test/hil" "$1/examples"
REMOTE

# Copy HIL test script and config
echo "==> Copying test scripts"
scp -q "$ROOT_DIR/test/hil/hil_test.py" \
       "$ROOT_DIR/test/hil/pymtp.py" \
       "$CONFIG" \
       "$REMOTE:$REMOTE_DIR/test/hil/"

# Copy only firmware binaries (elf/bin/hex) plus esptool metadata
# (config.env + flash_args needed by the esptool flasher), preserving structure
copy_board_binaries() {
  local src="$1"
  rsync -a --prune-empty-dirs \
    --include='*/' --include='*.elf' --include='*.bin' --include='*.hex' \
    --include='config.env' --include='flash_args' \
    --exclude='*' \
    "$src" "$REMOTE:$REMOTE_DIR/examples/"
}

if [ -n "$BOARD" ]; then
  # Copy the board's build dir plus its variant dirs. Variant names come from
  # $CONFIG (they are not required to be prefixed with the board name); the
  # cmake-build-<BOARD>-* glob is kept as a fallback for ad-hoc local builds.
  # Collect only dirs that actually exist, deduplicated.
  declare -A SEEN_DIRS=()
  BUILD_DIRS=()
  add_build_dir() {
    [[ -d "$1" && -z "${SEEN_DIRS[$1]:-}" ]] || return 0
    SEEN_DIRS[$1]=1
    BUILD_DIRS+=("$1")
  }
  shopt -s nullglob
  for d in "$ROOT_DIR"/examples/cmake-build-"$BOARD" "$ROOT_DIR"/examples/cmake-build-"$BOARD"-*; do
    add_build_dir "$d"
  done
  shopt -u nullglob
  while IFS= read -r v; do
    add_build_dir "$ROOT_DIR/examples/cmake-build-$v"
  done < <(python3 -c '
import json, sys
cfg = json.load(open(sys.argv[1]))
for b in cfg.get("boards", []):
    if b["name"] == sys.argv[2]:
        for v in b.get("variant") or []:
            print(v["name"])
' "$CONFIG" "$BOARD")
  if [ ${#BUILD_DIRS[@]} -eq 0 ]; then
    echo "Error: no build directory found for $BOARD under $ROOT_DIR/examples/"
    echo "Build first with: cd examples && cmake --preset $BOARD && cmake --build --preset $BOARD"
    exit 1
  fi
  echo "==> Copying binaries for $BOARD (${#BUILD_DIRS[@]} build dir(s))"
  for d in "${BUILD_DIRS[@]}"; do
    copy_board_binaries "$d"
  done
else
  echo "==> Copying all built binaries"
  # Use `%/` parameter expansion to strip the trailing slash from the glob —
  # rsync needs the bare dir name so the per-board cmake-build-<BOARD>/ subdir
  # is preserved on the remote (hil_test.py looks up binaries by that path).
  for dir in "$ROOT_DIR"/examples/cmake-build-*/; do
    [ -d "$dir" ] && copy_board_binaries "${dir%/}"
  done
fi

# Run test. Use `bash -s` so REMOTE_DIR + ARGS reach the remote shell as positional
# parameters; quoting and metacharacters in args are preserved.
CONFIG_BASENAME="$(basename "$CONFIG")"
echo "==> Running HIL test on $REMOTE"
rc=0
ssh "$REMOTE" bash -s -- "$REMOTE_DIR" "${ARGS[@]}" "test/hil/$CONFIG_BASENAME" <<'REMOTE' || rc=$?
cd -- "$1"
shift
# Flasher CLIs live in the user bin dirs on ci.lan (esptool/idf in ~/.local/bin,
# STM32CubeProgrammer's STM32_Programmer_CLI in ~/bin); the non-interactive shell
# subprocess used for flashing doesn't source profile/rc, so add them explicitly.
export PATH="$HOME/.local/bin:$HOME/bin:$PATH"
python3 -u test/hil/hil_test.py -B examples "$@"
REMOTE

# Copy the generated report back to the local checkout (best-effort; the run's
# exit code is preserved regardless of whether a report was produced).
scp -q "$REMOTE:$REMOTE_DIR/hil_report.md" "$ROOT_DIR/hil_report.md" \
  && echo "==> Report copied to $ROOT_DIR/hil_report.md" \
  || echo "==> warning: no hil_report.md copied back" >&2

exit $rc
