#!/usr/bin/env bash
# Run HIL test remotely on ci.lan
# Usage: test/hil/hil_ci.sh [-b BOARD]... [-t TEST] [extra hil_test.py args...]
# Example:
#   test/hil/hil_ci.sh -b stm32f723disco
#   test/hil/hil_ci.sh -b stm32f723disco -b raspberry_pi_pico
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

# Parse -b BOARD from arguments to know which builds to copy. Repeatable: hil_test.py
# takes the whole board set in ONE run (it schedules them across host controllers and
# budgets the flashes itself), so every -b needs its binaries staged, not just the last.
BOARDS=()
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    # hil_test.py declares `-b, --board` with action='append', so argparse also accepts
    # --board=X and -bX. Recognising only the bare `-b X` forwarded the others to the rig
    # while never staging them: the board ran with no firmware and reported a green row.
    -b|--board)
      [[ $# -ge 2 ]] || { echo "error: $1 requires a BOARD argument" >&2; exit 1; }
      BOARDS+=("$2")
      ARGS+=("$1" "$2")
      shift 2
      ;;
    --board=*)
      BOARDS+=("${1#--board=}")
      ARGS+=("$1")
      shift
      ;;
    # -bt (--board-test) BEFORE the glued -b?* arm, mirroring argparse's longest-match: it is
    # the form <config>.failed uses, and a bare -b?* would register a board named "t..." that
    # the roster check below rejects -- killing every documented retry.
    -bt|--board-test)
      [[ $# -ge 2 ]] || { echo "error: $1 requires NAME:tests" >&2; exit 1; }
      ARGS+=("$1" "$2")
      shift 2
      ;;
    -bt?*|--board-test=*)
      ARGS+=("$1")
      shift
      ;;
    # glued short form: argparse resolves -bNAME to --board NAME, so staging must too --
    # unparsed it fell through to the all-boards branch and silently staged everything built
    -b?*)
      BOARDS+=("${1#-b}")
      ARGS+=("$1")
      shift
      ;;
    *)
      ARGS+=("$1")
      shift
      ;;
  esac
done

# Resolve a board to its build dirs: its own dir, the cmake-build-<board>-* glob (ad-hoc
# local builds), and the variant dirs named in $CONFIG -- variant names are NOT required to
# be prefixed with the board name, so the glob alone is not enough. Prints one dir per line.
variant_names() {
  python3 -c '
import json, sys
cfg = json.load(open(sys.argv[1]))
for b in cfg.get("boards", []):
    if b["name"] == sys.argv[2]:
        for v in b.get("variant") or []:
            print(v["name"])
' "$CONFIG" "$1"
}

resolve_build_dirs() {
  local board="$1" d v
  declare -A seen=()
  shopt -s nullglob
  for d in "$ROOT_DIR"/examples/cmake-build-"$board" "$ROOT_DIR"/examples/cmake-build-"$board"-*; do
    [[ -d $d && -z ${seen[$d]:-} ]] && { seen[$d]=1; printf '%s\n' "$d"; }
  done
  shopt -u nullglob
  # to a file, not a process substitution: `set -e`/pipefail cannot see the exit status of
  # the latter, so a malformed roster silently yielded zero variant dirs
  local vf; vf=$(mktemp)
  variant_names "$board" > "$vf" || { rm -f "$vf"; echo "Error: could not read variants for $board from $CONFIG" >&2; exit 1; }
  while IFS= read -r v; do
    d="$ROOT_DIR/examples/cmake-build-$v"
    [[ -d $d && -z ${seen[$d]:-} ]] && { seen[$d]=1; printf '%s\n' "$d"; }
  done < "$vf"
  rm -f "$vf"
}

# Pre-flight: EVERY board must resolve to at least one build dir before anything is wiped or
# copied. This check used to live in the copy loop, so an unbuilt board late in the list
# aborted the run after the remote tree had been rm -rf'd and earlier boards fully rsynced --
# zero coverage, a half-staged rig, and a stale local hil_report.md left in place. Report all
# missing boards at once so one build round fixes them.
MANIFEST=$(mktemp)
trap 'rm -f "$MANIFEST"' EXIT
# Roster membership first: hil_test.py rejects an unknown -b with sys.exit(1) for the WHOLE
# run (hil_test.py:2297), and it does so AFTER this script has wiped REMOTE_DIR and staged
# every board -- one typo then costs the entire batch. We already parse $CONFIG here, so
# catch it before anything is touched. Note -b matches board names only, never variant names.
if [ ${#BOARDS[@]} -gt 0 ]; then
ROSTER=$(python3 -c '
import json, sys
print("\n".join(b["name"] for b in json.load(open(sys.argv[1])).get("boards", [])))
' "$CONFIG") || { echo "error: could not read the board roster from $CONFIG" >&2; exit 1; }
notinroster=()
for b in ${BOARDS[@]+"${BOARDS[@]}"}; do
  grep -qxF -- "$b" <<< "$ROSTER" || notinroster+=("$b")
done
if [ ${#notinroster[@]} -gt 0 ]; then
  echo "error: not in $(basename "$CONFIG"): ${notinroster[*]}" >&2
  echo "       (-b takes board names, not variant names)" >&2
  exit 1
fi
fi   # BOARDS non-empty: nothing to validate for an all-boards run

missing=()
for b in ${BOARDS[@]+"${BOARDS[@]}"}; do
  dirs=$(resolve_build_dirs "$b")
  if [ -z "$dirs" ]; then
    missing+=("$b")
  else
    while IFS= read -r d; do printf '%s\t%s\n' "$b" "$d" >> "$MANIFEST"; done <<< "$dirs"
    # A declared variant with no build dir is NOT an error -- no cmake preset is
    # variant-suffixed, so this is the normal state for e.g. the -DMA variants. It is worth
    # saying out loud: hil_test.py logs `Skip (no binary)` and counts zero errors for it, so
    # the run exits 0 and the operator reads a green table for cells that never ran.
    # plain assignment, not process substitution: set -e sees a variant_names failure here,
    # the same trap the comment in resolve_build_dirs warns about
    vnames=$(variant_names "$b")
    while IFS= read -r v; do
      [ -z "$v" ] && continue
      # whole lines: a substring match lets cmake-build-<v>-DMA silence the warning for <v>
      grep -qxF -- "$ROOT_DIR/examples/cmake-build-$v" <<< "$dirs" \
        || echo "warning: $b variant '$v' has no build dir -- its cells will be skipped, not tested" >&2
    done <<< "$vnames"
  fi
done
if [ ${#missing[@]} -gt 0 ]; then
  echo "Error: no build directory under $ROOT_DIR/examples/ for: ${missing[*]}" >&2
  for b in "${missing[@]}"; do
    echo "  cd examples && cmake --preset $b && cmake --build --preset $b" >&2
  done
  exit 1
fi

# The all-boards form needs its emptiness check HERE too: below the setup ssh it fired after
# the remote tree was already rm -rf'd, destroying the previous run's report and re-run spec
# on the rig before deciding there was nothing to do.
if [ ${#BOARDS[@]} -eq 0 ]; then
  shopt -s nullglob
  allbuilds=("$ROOT_DIR"/examples/cmake-build-*/)
  shopt -u nullglob
  if [ ${#allbuilds[@]} -eq 0 ]; then
    echo "error: no examples/cmake-build-* directories under $ROOT_DIR -- nothing to test" >&2
    echo "       build first, e.g.: cd examples && cmake --preset <board> && cmake --build --preset <board>" >&2
    exit 1
  fi
fi

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
       "$ROOT_DIR/test/hil/helper/hil_summary.py" \
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

if [ ${#BOARDS[@]} -gt 0 ]; then
  # Replay the pre-flight manifest: the dirs were already resolved and proved non-empty
  # for every board, so nothing here can abort mid-staging. Plain reads of the manifest --
  # a process substitution would hide a reader failure from set -e (the comment in
  # resolve_build_dirs is about exactly that trap).
  for b in "${BOARDS[@]}"; do
    dirs=()
    while IFS=$'\t' read -r bb d; do
      [ "$bb" = "$b" ] && [ -n "$d" ] && dirs+=("$d")
    done < "$MANIFEST"
    echo "==> Copying binaries for $b (${#dirs[@]} build dir(s))"
    for d in ${dirs[@]+"${dirs[@]}"}; do
      copy_board_binaries "$d"
    done
  done
else
  # emptiness was already refused in pre-flight, before the remote wipe
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
# --retry 1 FIRST, before the user's args: this targets the same shared rig CI uses, and the
# pool guard is a flat constant that does not scale with max_retry, so a few flaky boards can
# re-pay ~510s each until the 3600s guard fires, abandoning the pool and holding board flocks
# against concurrent CI. hil_test.py's own default is already 1; passing it explicitly keeps
# that true if the default ever moves. Placed first, not appended, so argparse's last-wins
# means `hil_ci.sh -r 3` still gets 3.
# Forward the HIL_* knobs (HIL_NO_BOARD_LOCK for an authorized force, the parallel widths,
# HIL_POOL_TIMEOUT). ssh passes no environment and joins its argv into one string the remote
# shell re-splits, so a bare NAME=value element would arrive as a positional argument to
# hil_test.py and argparse would exit 2. Build `export` lines instead and hand them over as a
# single %q-quoted word for the remote to eval.
# Joined with '; ', NOT newlines: %q renders a newline as bash-only $'...' quoting, which the
# remote LOGIN shell must parse from the joined command string -- under dash the force arrives
# as garbage and silently does nothing. Backslash escaping round-trips in both shells.
# HIL_REPORT_DIR stays local: where the report lands on the rig is this script's contract
# (REMOTE_DIR, where all three copy-backs below look), so forwarding it would relocate the
# report and every copy-back would come home empty.
HIL_EXPORTS=""
while IFS= read -r v; do
  [ -z "$v" ] && continue
  HIL_EXPORTS+="export $(printf '%s=%q' "$v" "${!v}"); "
done < <(compgen -v | grep -x 'HIL_[A-Z0-9_]*' | grep -vxE 'HIL_EXPORTS|HIL_REPORT_DIR' || true)
[ -n "$HIL_EXPORTS" ] && echo "==> Forwarding: $HIL_EXPORTS"
# One %q-quoted word, so ssh's argv join and the remote shell's re-split hand it back
# byte-for-byte, and the remote evals it. Empty stays `''` -- a real, shiftable argument --
# rather than vanishing from the joined string and shifting the run's own flags out of place.
HIL_EXPORTS_Q=$(printf '%q' "$HIL_EXPORTS")

ssh "$REMOTE" bash -s -- "$REMOTE_DIR" "$HIL_EXPORTS_Q" --retry 1 ${ARGS_Q[@]+"${ARGS_Q[@]}"} "$CONFIG_Q" <<'REMOTE' || rc=$?
cd -- "$1"
shift
eval "$1"; shift        # HIL_* exports, %q-quoted locally into one word
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

# The re-run spec and the JSON sidecar live in the run's cwd on the rig (REMOTE_DIR), and the
# next invocation rm -rf's it. Without copying them back, the `--accumulate` retry every doc on
# this branch prescribes has nothing to read and nothing to merge onto. Delete the local copies
# FIRST: a green run writes no .failed, so a silent no-op scp would leave last run's spec in
# the checkout looking current, and "retry from the spec" would re-flash boards that passed.
for extra in "$(basename "$CONFIG").failed" hil_report.json; do
  rm -f "$ROOT_DIR/$extra"
  scp -q "$REMOTE:$REMOTE_DIR/$extra" "$ROOT_DIR/$extra" 2>/dev/null \
    && echo "==> $extra copied to $ROOT_DIR/$extra" || true
done

exit $rc
