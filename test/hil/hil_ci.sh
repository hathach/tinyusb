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

# REMOTE_DIR reaches the rig as `rm -rf` input, an scp remote path and an rsync remote
# path -- the remote shell re-splits and expands all three, so no amount of LOCAL quoting
# protects them (and %q would escape the ~ that REMOTE_DIR=~/dir needs). Screen it once.
# The tilde is the whole hazard: the REMOTE shell expands it, so `~/` alone -- one typo
# away from the documented ~/dir override -- means `rm -rf` on that account's HOME. Hence
# `/` or `~/` followed by at least one named component, ending in a name character.
[[ $REMOTE_DIR =~ ^(/|~/)[A-Za-z0-9_.~/-]*[A-Za-z0-9_-]$ && $REMOTE_DIR != *..*
   && $REMOTE_DIR != *//* ]] || {
  echo "error: REMOTE_DIR must be /path or ~/path of [A-Za-z0-9_.~/-], no '..', no" \
       "trailing slash -- it is an rm -rf target on $REMOTE: $REMOTE_DIR" >&2
  exit 1
}

# --build would run tools/build.py ON THE RIG, and this script stages binaries, not the
# build tree -- it is not copied, so the run dies there with a confusing missing-file
# error. Building is the local half of this workflow by design.
for a in "$@"; do
  [ "$a" = "--build" ] || continue
  echo "error: --build builds on the REMOTE, but this script copies prebuilt binaries" >&2
  echo "       (tools/build.py is not staged). Build locally first, then re-run:" >&2
  echo "       cd examples && cmake --preset <board> && cmake --build --preset <board>" >&2
  exit 1
done

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

# Setup remote directory. `bash -s` + heredoc so REMOTE_DIR arrives as a positional
# parameter, keeping the `rm -rf` target out of the command string the heredoc runs.
echo "==> Setting up remote $REMOTE:$REMOTE_DIR"
ssh "$REMOTE" bash -s -- "$REMOTE_DIR" <<'REMOTE'
set -e
# Second gate, on the side that knows what ~ expanded to: only here is $HOME a value
# rather than a guess, and this is the line that actually runs rm -rf.
case "$1" in
  ''|/|"$HOME"|"$HOME"/) echo "refusing to rm -rf '$1'" >&2; exit 1 ;;
esac
rm -rf -- "$1"
mkdir -p -- "$1/test/hil/helper" "$1/examples"
REMOTE

# Copy HIL test script and config
echo "==> Copying test scripts"
scp -q "$ROOT_DIR/test/hil/hil_test.py" \
       "$ROOT_DIR/test/hil/hil_flash.py" \
       "$ROOT_DIR/test/hil/usbtest.py" \
       "$ROOT_DIR/test/hil/pymtp.py" \
       "$ROOT_DIR/test/hil/mtp_test.py" \
       "$CONFIG" \
       "$REMOTE:$REMOTE_DIR/test/hil/"
scp -q "$ROOT_DIR/test/hil/helper/__init__.py" \
       "$ROOT_DIR/test/hil/helper/hil_util.py" \
       "$ROOT_DIR/test/hil/helper/hil_health.py" \
       "$ROOT_DIR/test/hil/helper/hil_lock.py" \
       "$ROOT_DIR/test/hil/helper/hil_select.py" \
       "$REMOTE:$REMOTE_DIR/test/hil/helper/"

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
  # to a file, not a process substitution: `set -e`/pipefail cannot see the exit
  # status of the latter, so a malformed roster silently yielded zero variant dirs
  VARIANTS_FILE=$(mktemp)
  python3 -c '
import json, sys
cfg = json.load(open(sys.argv[1]))
for b in cfg.get("boards", []):
    if b["name"] == sys.argv[2]:
        for v in b.get("variant") or []:
            print(v["name"])
' "$CONFIG" "$BOARD" > "$VARIANTS_FILE" || {
    echo "Error: could not read variants for $BOARD from $CONFIG"
    rm -f "$VARIANTS_FILE"
    exit 1
  }
  while IFS= read -r v; do
    add_build_dir "$ROOT_DIR/examples/cmake-build-$v"
  done < "$VARIANTS_FILE"
  rm -f "$VARIANTS_FILE"
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

# Run test via `bash -s`, so REMOTE_DIR and the args arrive as positional parameters.
# %q the ARGS -- ssh joins its argv into ONE string that the remote shell re-splits, so
# `-t 'host/cdc msc'` would arrive as two arguments and hil_test.py would see a stray
# word where it expects the config path. REMOTE_DIR is deliberately NOT quoted here: it
# is screened above precisely so it can keep its ~ expansion.
ARGS_Q=()
for a in ${ARGS[@]+"${ARGS[@]}"}; do ARGS_Q+=("$(printf '%q' "$a")"); done
# same re-split, same fix: CONFIG is a user-supplied path and its basename lands in the
# command string too
CONFIG_Q="$(printf '%q' "test/hil/$(basename "$CONFIG")")"
echo "==> Running HIL test on $REMOTE"
rc=0
# --retry 1 FIRST, before the user's args: this targets the same shared rig CI uses, and
# the pool guard is a flat constant that does not scale with max_retry -- argparse's
# default of 3 lets a few flaky boards re-pay 510s each until the 3600s guard fires,
# abandoning the pool and holding board flocks against concurrent CI. Placed first, not
# appended, so argparse's last-wins means `hil_ci.sh -r 3` still gets 3.
ssh "$REMOTE" bash -s -- "$REMOTE_DIR" --retry 1 ${ARGS_Q[@]+"${ARGS_Q[@]}"} "$CONFIG_Q" <<'REMOTE' || rc=$?
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
