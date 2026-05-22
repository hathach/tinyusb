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

# Copy only firmware binaries (elf/bin/hex), preserving directory structure
copy_board_binaries() {
  local src="$1"
  rsync -a --prune-empty-dirs \
    --include='*/' --include='*.elf' --include='*.bin' --include='*.hex' --exclude='*' \
    "$src" "$REMOTE:$REMOTE_DIR/examples/"
}

if [ -n "$BOARD" ]; then
  BUILD_DIR="$ROOT_DIR/examples/cmake-build-$BOARD"
  if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: build directory not found: $BUILD_DIR"
    echo "Build first with: cd examples && cmake -DBOARD=$BOARD -G Ninja -B cmake-build-$BOARD . && cmake --build cmake-build-$BOARD"
    exit 1
  fi
  echo "==> Copying binaries for $BOARD"
  copy_board_binaries "$BUILD_DIR"
else
  echo "==> Copying all built binaries"
  for dir in "$ROOT_DIR"/examples/cmake-build-*/; do
    [ -d "$dir" ] && copy_board_binaries "$dir"
  done
fi

# Run test. Use `bash -s` so REMOTE_DIR + ARGS reach the remote shell as positional
# parameters; quoting and metacharacters in args are preserved.
CONFIG_BASENAME="$(basename "$CONFIG")"
echo "==> Running HIL test on $REMOTE"
ssh "$REMOTE" bash -s -- "$REMOTE_DIR" "${ARGS[@]}" "test/hil/$CONFIG_BASENAME" <<'REMOTE'
cd -- "$1"
shift
exec python3 -u test/hil/hil_test.py -B examples "$@"
REMOTE
