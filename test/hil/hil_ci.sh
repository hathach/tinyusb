#!/bin/bash
# Run HIL test remotely on ci.lan
# Usage: test/hil/hil_ci.sh [-b BOARD] [-t TEST] [extra hil_test.py args...]
# Example:
#   test/hil/hil_ci.sh -b stm32f723disco
#   test/hil/hil_ci.sh -b stm32f723disco -t host/cdc_msc_hid -r 1

set -e

REMOTE=ci.lan
REMOTE_DIR=/tmp/tinyusb-hil
SCRIPT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

# Parse -b BOARD from arguments to know which build to copy
BOARD=""
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -b)
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

# Setup remote directory
echo "==> Setting up remote $REMOTE:$REMOTE_DIR"
ssh "$REMOTE" "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR/test/hil $REMOTE_DIR/examples"

# Copy HIL test script and config
echo "==> Copying test scripts"
scp -q "$SCRIPT_DIR/test/hil/hil_test.py" \
       "$SCRIPT_DIR/test/hil/pymtp.py" \
       "$SCRIPT_DIR/test/hil/tinyusb.json" \
       "$REMOTE:$REMOTE_DIR/test/hil/"

# Copy only firmware binaries (elf/bin/hex), preserving directory structure
copy_board_binaries() {
  local src="$1"
  local board_name
  board_name=$(basename "$src")
  rsync -a --include='*/' --include='*.elf' --include='*.bin' --include='*.hex' --exclude='*' \
    "$src" "$REMOTE:$REMOTE_DIR/examples/"
}

if [ -n "$BOARD" ]; then
  BUILD_DIR="$SCRIPT_DIR/examples/cmake-build-$BOARD"
  if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: build directory not found: $BUILD_DIR"
    echo "Build first with: cd examples && cmake -DBOARD=$BOARD -G Ninja -B cmake-build-$BOARD .. && cmake --build cmake-build-$BOARD"
    exit 1
  fi
  echo "==> Copying binaries for $BOARD"
  copy_board_binaries "$BUILD_DIR"
else
  echo "==> Copying all built binaries"
  for dir in "$SCRIPT_DIR"/examples/cmake-build-*/; do
    [ -d "$dir" ] && copy_board_binaries "$dir"
  done
fi

# Run test
echo "==> Running HIL test on $REMOTE"
ssh -t "$REMOTE" "cd $REMOTE_DIR && python3 -u test/hil/hil_test.py -B examples ${ARGS[*]} tinyusb.json"
