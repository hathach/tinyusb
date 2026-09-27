#!/usr/bin/env bash
# usb_recover.sh — USB recovery helper for the HIL rig; run with sudo. Writes only
# to the specific sysfs control files below; arg regexes block path traversal.
#
# Usage, from the checkout root (sudo's PATH does not include the script):
#   sudo .claude/skills/usb-kernel-recover/scripts/usb_recover.sh <verb> ...
#   authorized <busport>   # e.g. 3-2  -> deauthorize+reauthorize (re-enumerate, NO VBUS cut)
#   root-cycle <busport> [serial]  # e.g. 13-1.6 -> uhubctl port-off/on at the ROOT port feeding
#                          # it; [serial] is verified against the device and refused on mismatch,
#                          # skipping the leaf hubs (which fake ganged switching and do not
#                          # actually cut power). Bounces every sibling under that root port.
#                          # The D-state escape: no device lock, so it cannot convoy.
#   pci-rebind <pciaddr>   # e.g. 0000:05:00.0 -> unbind+bind the whole xHCI
#                          # controller. For a DEAD CONTROLLER, not a wedged
#                          # device: it renumbers every bus it owns.
#   pci-bind   <pciaddr> [drv]  # re-attach a driver to a DRIVERLESS controller
#   resolve    <devnode>   # e.g. /dev/ttyACM3 -> print its <busport> (no privilege needed)
#   shield     <busport> <owner-pid>    # chmod 000 the nine locking attrs of the wedged leaf, its
#                          # parent hub and the root hub so non-root libusb enumerators skip it;
#                          # records every original mode first. Refused while another shield
#                          # covers any of the same objects.
#   unshield   <busport> [owner-pid]    # restore the recorded modes on the surviving originals
#                          # (same inode) and drop the record; refused while a DIFFERENT owner
#                          # is still alive. A stale record (owner gone) needs no pid.
#   shield-status [busport] # records, their owners (alive/dead) and what is still shielded
set -euo pipefail

# The shield's locking attributes: served under the device lock, so a wedged device blocks
# every reader (usb-kernel-recover SKILL.md section 2). descriptors/busnum/devnum/speed/
# idVendor/idProduct are lock-free and libusb needs them -- never in this list.
SHIELD_ATTRS='bNumInterfaces bmAttributes bMaxPower configuration bConfigurationValue product manufacturer serial avoid_reset_quirk'
# Test seams, honoured only WITHOUT root: a fake sysfs tree and a scratch state dir. Under
# sudo they are ignored, so the script cannot be pointed at arbitrary paths as a privileged chmod.
SYSFS=/sys
SHIELD_STATE=/run/tinyusb-hil/shield
FAIL_CHMOD=''          # a path whose chmod the tests make fail, to reach the rollback-failure branch
if [ "$(id -u)" -ne 0 ]; then
  SYSFS=${USB_RECOVER_SYSFS:-/sys}
  SHIELD_STATE=${USB_RECOVER_STATE:-/run/tinyusb-hil/shield}
  FAIL_CHMOD=${USB_RECOVER_TEST_FAIL_CHMOD:-}
fi

USBPATH_RE='^[0-9]+-[0-9]+(\.[0-9]+)*$'
PCI_RE='^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9]$'
DRIVER_RE='^[A-Za-z0-9_-]+$'

die() { echo "usb_recover: $*" >&2; exit 1; }

lock_read() {
  # Read an attribute served under the device lock (serial, product) with a 2s bound.
  # Prints the value, '' when the attribute is absent, or '?' when it did not answer.
  #
  # Bounding these is load-bearing, not defensive: they are the FIRST thing root-cycle
  # does, so on a real wedge an unbounded read blocks before reaching uhubctl at all
  # (observed live: one attempt sat 3h; three concurrent invocations all frozen there).
  # The operator then reads that as "recovery didn't work" and escalates to a bare
  # `uhubctl -a cycle`, which tears the subtree down and blocks holding the ROOT HUB
  # lock -- taking the whole bus with it. That is how one wedge becomes an incident.
  #
  # `timeout` is enough, though this said for a while that it was not (claiming the read
  # sits in D state, where SIGKILL is not delivered, so timeout waitpid()s forever). It
  # does not: v6.12.101 drivers/usb/core/sysfs.c takes the lock for every READ through
  # usb_lock_device_interruptible -> device_lock_interruptible -> mutex_lock_interruptible,
  # so the waiter sleeps INTERRUPTIBLY and SIGTERM ends it. Uninterruptible is the usbfs
  # ioctl HOLDER, not us. The abandon-a-background-reader dance that claim justified is
  # gone, and with it a fail-open where an absent attribute answered '?' -- the wedge
  # signature, which root-cycle reads as "cannot confirm serial, proceed".
  local v rc=0
  # `|| rc=$?`, never a bare assignment: under this script's `set -e` a command
  # substitution that FAILS (an absent attribute -- most hubs and probes have no
  # iSerialNumber, and `product` is often missing) exits the whole recovery script.
  v=$(timeout 2 cat "$1" 2>/dev/null) || rc=$?
  [ "$rc" -eq 124 ] && { echo '?'; return; }      # timed out: nobody answered
  printf '%s\n' "$v"
}

# Generation marker for "did this device actually re-enumerate". A real disconnect destroys the
# usb_device and its sysfs kobject; reconnecting creates a new one, and kernfs hands out inode
# numbers monotonically, so the directory inode changes. Verified on the rig: ports re-enumerated
# minutes ago carry inodes in the millions while ports untouched since boot are still in the tens
# of thousands, ranking identically to their mtimes.
#
# This beats comparing devnum, which Linux reuses once the per-bus map wraps (observed live: a
# single cycle moved one device 123 -> 113). It also beats watching for the node to vanish, since
# `uhubctl -a cycle` holds the whole power-off window inside itself and a poll afterwards can
# never witness the gap. The inode survives the gap, so no observation window is needed.
#
# Crucially, if the disconnect is blocked on the wedged device's lock the kobject is never
# recreated -- same inode -- which is exactly the case that must be reported as a failure. Verified
# against kernfs: __kernfs_new_node() allocates via idr_alloc_cyclic() but kernfs_id_ino() exposes
# the full 64-bit (id_highbits<<32 | lowbits) as st_ino on 64-bit ino_t, so a repeat needs ~2^64
# node creations. authorized-toggle, set_configuration and suspend/resume all leave the parent
# device kobject alone, so none of them can move the marker and fake a success.
#
# The trailing slash is load-bearing: /sys/bus/usb/devices/<busport> is a SYMLINK with its own
# separate inode, so without it stat reports the link rather than the device it points at, and the
# value would never change. Do not "tidy" it away.
# Refuse to touch a PCI function that is not a USB controller (class 0x0c03xx), so a stray or
# mistyped BDF can't unbind/reset an unrelated device (storage, NIC) on a shared HIL host.
require_usb_controller() {
  local addr=$1 cls
  cls=$(cat "/sys/bus/pci/devices/$addr/class" 2>/dev/null) || die "no such pci device: $addr"
  [[ "$cls" =~ ^0x0c03 ]] || die "$addr is not a USB controller (class $cls); refusing"
}

sysfs_gen() { stat -c %i "/sys/bus/usb/devices/$1/" 2>/dev/null || echo none; }
usage() { sed -n '/^# Usage/,/^set -euo pipefail/p' "$0" | grep -E '^#   (sudo|[a-z])' >&2; exit 2; }

# ---- shield -----------------------------------------------------------------------------
# One record per shielded busport under $SHIELD_STATE, written BEFORE the first chmod:
#   owner <pid> <start-time> <boot-id>
#   obj <name> <dir-inode>            (leaf, parent hub, root hub)
#   attr <name>/<attr> <inode> <mode>
# The inode is the object's generation: re-enumeration destroys the kobject and its
# attributes and creates new ones, so unshield restores only what still carries the
# recorded inode and reports the rest as gone. Modes are restored from the record, never
# copied from a sibling (siblings differ, and may carry another shield). Every check and
# mutation runs under one flock, so two shields cannot both pass the overlap check.

attr_op() {
  # attr_op stat PATH            -> "<inode> <mode>"      rc 0
  # attr_op chmod PATH INODE MODE -> "ok"                 rc 0, applied to THAT inode
  # rc 2 "gone": the path is absent or names a different inode (re-enumerated, replaced)
  # rc 1: could not inspect or change it -- never mistaken for gone
  # O_PATH|O_NOFOLLOW pins the object without opening it for reading (no ->show() on a
  # wedged attribute, no read permission needed), fstat gives the inode of exactly that
  # object, and chmod through /proc/self/fd changes it and nothing that replaced it.
  FAIL_CHMOD="$FAIL_CHMOD" python3 - "$@" <<'PY'
import os, stat, sys
op, path = sys.argv[1], sys.argv[2]
if op == 'chmod' and path == os.environ.get('FAIL_CHMOD') and sys.argv[4] != '000':
    print('chmod failed: injected'); sys.exit(1)     # a RESTORE that fails, the rollback branch
try:
    fd = os.open(path, os.O_PATH | os.O_NOFOLLOW | os.O_CLOEXEC)
except FileNotFoundError:
    print('gone'); sys.exit(2)
except OSError as e:
    print(f'inspect failed: {e.strerror}'); sys.exit(1)
try:
    st = os.fstat(fd)
    if not stat.S_ISREG(st.st_mode):
        print('not a regular file'); sys.exit(1)
    if op == 'stat':
        print(f'{st.st_ino} {st.st_mode & 0o777:o}'); sys.exit(0)
    if str(st.st_ino) != sys.argv[3]:
        print('gone'); sys.exit(2)
    if st.st_mode & 0o777 == int(sys.argv[4], 8):
        print('ok'); sys.exit(0)        # already there: nothing to change, nothing to fail
    try:
        os.chmod(f'/proc/self/fd/{fd}', int(sys.argv[4], 8))
    except OSError as e:
        print(f'chmod failed: {e.strerror}'); sys.exit(1)
    print('ok')
finally:
    os.close(fd)
PY
}

shield_objects() {
  # leaf, its parent hub, the root hub -- unique, in that order
  local bp=$1 bus=${1%%-*} parent
  if [[ "$bp" == *.* ]]; then parent=${bp%.*}; else parent="usb$bus"; fi
  echo "$bp"
  [ "$parent" != "usb$bus" ] && echo "$parent"
  echo "usb$bus"
}

proc_start() {  # start time of a pid (clock ticks since boot), '' when it is gone
  local stat
  stat=$(cat "/proc/$1/stat" 2>/dev/null) || return 0
  stat=${stat##*) }          # skip past the comm field, which may contain spaces
  set -- $stat
  echo "${20:-}"             # field 22 of the full line
}

owner_alive() {  # pid start boot -> 0 when that very process still runs
  [ "$3" = "$(cat /proc/sys/kernel/random/boot_id)" ] || return 1
  [ -n "$2" ] && [ "$(proc_start "$1")" = "$2" ]
}

record_owner() {  # the record's "owner pid start boot" fields, plus alive|dead
  local pid start boot
  read -r _ pid start boot < <(grep -m1 '^owner ' "$1")
  if owner_alive "$pid" "$start" "$boot"; then echo "$pid $start $boot alive"; else echo "$pid $start $boot dead"; fi
}

shield_lock() {
  mkdir -p -m 700 "$SHIELD_STATE" || die "cannot create $SHIELD_STATE"
  exec 9>>"$SHIELD_STATE/.lock" || die "cannot open $SHIELD_STATE/.lock"
  flock -w 30 9 || die "another shield/unshield has held $SHIELD_STATE/.lock for 30 s"
}

# restore_from LINES... : chmod each "attr <obj/f> <ino> <mode>" line back to <mode>.
# Sets restored/gone and appends failures to failed[].
restore_from() {
  local line out rc
  for line in "$@"; do
    set -- $line
    rc=0; out=$(attr_op chmod "$SYSFS/bus/usb/devices/$2" "$3" "$4") || rc=$?   # never a bare assignment under set -e
    case $rc in
      0) restored=$((restored + 1)) ;;
      2) gone=$((gone + 1)) ;;
      *) failed+=("$2 ($out)") ;;
    esac
  done
}

shield() {
  local bp=$1 pid=$2 start objs obj rec tmp d f path out rc changed=() line
  local restored=0 gone=0 failed=()
  [[ "$bp" =~ $USBPATH_RE ]] || die "bad usb path: $bp"
  [[ "$pid" =~ ^[0-9]+$ ]] || die "shield: owner pid '$pid' is not a pid"
  start=$(proc_start "$pid")
  [ -n "$start" ] || die "shield: owner pid $pid is not a running process"
  objs=$(shield_objects "$bp")
  for obj in $objs; do
    [ -d "$SYSFS/bus/usb/devices/$obj/" ] || die "shield: no such usb object: $obj"
  done
  shield_lock
  # overlap: one owner's unshield must never drop another's root-hub shield
  for rec in "$SHIELD_STATE"/*; do
    [ -f "$rec" ] || continue
    for obj in $objs; do
      if grep -q "^obj $obj " "$rec"; then
        die "shield: $obj is already covered by the shield on $(basename "$rec") (owner $(record_owner "$rec")); refusing"
      fi
    done
  done
  rec="$SHIELD_STATE/$bp"
  # snapshot every attribute before anything is published or changed; an attribute that
  # cannot be inspected stops the shield here, with nothing done
  tmp=$(mktemp "$SHIELD_STATE/.$bp.XXXXXX")
  {
    echo "owner $pid $start $(cat /proc/sys/kernel/random/boot_id)"
    for obj in $objs; do
      d="$SYSFS/bus/usb/devices/$obj"
      echo "obj $obj $(stat -c %i "$d/")"
      for f in $SHIELD_ATTRS; do
        rc=0; out=$(attr_op stat "$d/$f") || rc=$?
        case $rc in
          0) echo "attr $obj/$f $out" ;;
          2) echo "absent $obj/$f" ;;
          *) rm -f "$tmp"; die "shield: cannot inspect $obj/$f ($out); nothing changed" ;;
        esac
      done
    done
  } > "$tmp" || { rm -f "$tmp"; exit 1; }
  mv "$tmp" "$rec"
  # mutate only the inodes just recorded, and roll back on the first failure so a
  # half-shield never survives; a rollback that itself fails keeps the record
  while read -r line; do
    set -- $line
    [ "$1" = attr ] || continue
    rc=0; out=$(attr_op chmod "$SYSFS/bus/usb/devices/$2" "$3" 000) || rc=$?
    if [ $rc -ne 0 ]; then
      restore_from "${changed[@]}"
      if [ ${#failed[@]} -gt 0 ]; then
        die "shield: chmod 000 $2 failed ($out); rollback incomplete: ${failed[*]}; record $rec KEPT for unshield"
      fi
      rm -f "$rec"
      die "shield: chmod 000 $2 failed ($out); rolled back $restored attribute(s), no record kept"
    fi
    changed+=("$line")
  done < "$rec"
  echo "shielded $bp: ${#changed[@]} attribute(s) on $(echo $objs | tr ' ' ,) set 000; record $rec (owner pid $pid)"
}

unshield() {
  local bp=$1 pid=${2:-} rec owner line lines=()
  local restored=0 gone=0 failed=()
  [[ "$bp" =~ $USBPATH_RE ]] || die "bad usb path: $bp"
  shield_lock
  rec="$SHIELD_STATE/$bp"
  [ -f "$rec" ] || die "unshield: no shield record for $bp under $SHIELD_STATE"
  owner=$(record_owner "$rec")
  set -- $owner
  if [ "$4" = alive ] && [ "$pid" != "$1" ]; then
    die "unshield: $bp is shielded by live pid $1; pass that pid, or wait for it"
  fi
  while read -r line; do
    [[ "$line" == attr\ * ]] && lines+=("$line")
  done < "$rec"
  restore_from "${lines[@]}"
  if [ ${#failed[@]} -gt 0 ]; then
    die "unshield: could not restore ${failed[*]}; record $rec kept (restored $restored, gone $gone)"
  fi
  rm -f "$rec"
  echo "unshielded $bp: restored $restored attribute(s), $gone gone (re-enumerated or unplugged); record removed"
}

shield_status() {
  local only=${1:-} rec bp line n=0 still open gone unknown out rc
  for rec in "$SHIELD_STATE"/*; do
    [ -f "$rec" ] || continue
    bp=$(basename "$rec")
    [ -z "$only" ] || [ "$bp" = "$only" ] || continue
    n=$((n + 1)); still=0; open=0; gone=0; unknown=0
    while read -r line; do
      set -- $line
      [ "$1" = attr ] || continue
      rc=0; out=$(attr_op stat "$SYSFS/bus/usb/devices/$2") || rc=$?
      if [ $rc -eq 0 ] && [ "${out%% *}" = "$3" ]; then
        if [ "${out##* }" = 0 ]; then still=$((still + 1)); else open=$((open + 1)); fi
      elif [ $rc -eq 1 ]; then unknown=$((unknown + 1))
      else gone=$((gone + 1)); fi
    done < "$rec"
    set -- $(record_owner "$rec")
    echo "$bp: owner pid $1 $4; $still attribute(s) still shielded, $open recorded but not shielded, $gone gone, $unknown uninspectable; objects: $(grep '^obj ' "$rec" | cut -d' ' -f2 | tr '\n' ' ')"
  done
  [ $n -gt 0 ] || echo "no shields recorded under $SHIELD_STATE"
}

# Resolve a /dev node (ttyACMx, ttyUSBx, sgN, ...) up to its USB device busport.
resolve() {
  local node=$1 syspath dev
  [ -e "$node" ] || die "no such device node: $node"
  syspath=$(udevadm info -q path -n "$node" 2>/dev/null) || die "udevadm failed for $node"
  dev="/sys$syspath"
  while [ "$dev" != "/sys" ] && [ -n "$dev" ]; do
    if [ -e "$dev/busnum" ] && [ -e "$dev/devnum" ] && [ -e "$dev/authorized" ]; then
      basename "$dev"; return 0
    fi
    dev=$(dirname "$dev")
  done
  die "could not find parent USB device for $node"
}

action=${1:-}; target=${2:-}
[ -n "$action" ] || usage
[ -n "$target" ] || [ "$action" = shield-status ] || usage

case "$action" in
  shield)
    shield "$target" "${3:-}"
    ;;
  unshield)
    unshield "$target" "${3:-}"
    ;;
  shield-status)
    shield_status "$target"
    ;;
  resolve)
    resolve "$target"
    ;;
  authorized)
    [[ "$target" =~ $USBPATH_RE ]] || die "bad usb path: $target"
    d="/sys/bus/usb/devices/$target"
    [ -e "$d/authorized" ] || die "no such usb device: $target"
    echo 0 > "$d/authorized"; sleep 1; echo 1 > "$d/authorized"
    echo "re-authorized $target"
    ;;
  pci-rebind)
    [[ "$target" =~ $PCI_RE ]] || die "bad pci addr: $target"
    require_usb_controller "$target"
    [ -e "/sys/bus/pci/devices/$target/driver" ] || die "no driver bound to $target"
    drv=$(basename "$(readlink -f "/sys/bus/pci/devices/$target/driver")")
    echo "$target" > "/sys/bus/pci/drivers/$drv/unbind"; sleep 1
    echo "$target" > "/sys/bus/pci/drivers/$drv/bind"
    echo "rebound pci $target ($drv)"
    ;;
  pci-bind)
    # Re-attach a driver to a controller left DRIVERLESS (e.g. a pci-rebind whose re-bind hung).
    [[ "$target" =~ $PCI_RE ]] || die "bad pci addr: $target"
    require_usb_controller "$target"
    [ -e "/sys/bus/pci/devices/$target" ] || die "no such pci device: $target"
    [ -e "/sys/bus/pci/devices/$target/driver" ] && die "$target already has a driver bound"
    drv=${3:-}
    if [ -n "$drv" ]; then
      [[ "$drv" =~ $DRIVER_RE ]] || die "bad driver name: $drv"
      [ -e "/sys/bus/pci/drivers/$drv/bind" ] || die "no such pci driver: $drv"
      echo "$target" > "/sys/bus/pci/drivers/$drv/bind"
      echo "bound pci $target ($drv)"
    else
      # Auto-try the xHCI drivers (Renesas uPD720201 uses xhci-pci-renesas; others xhci_hcd).
      for cand in xhci-pci-renesas xhci_hcd; do
        [ -e "/sys/bus/pci/drivers/$cand/bind" ] || continue
        if echo "$target" > "/sys/bus/pci/drivers/$cand/bind" 2>/dev/null; then
          echo "bound pci $target ($cand)"; exit 0
        fi
      done
      die "could not bind $target with a known xHCI driver; pass the driver explicitly"
    fi
    ;;
  root-cycle)
    # Port-off/on at the ROOT port. NOTE: the Renesas ppps only disables D+/D- (VBUS stays up),
    # so this is a forced re-enumeration, not a power cycle. It goes straight at the root port --
    # no leaf walk (the 1a40:0201 hubs claim ganged switching but never cut power) -- and never
    # writes the wedged device's sysfs or takes its lock, so it cannot join a D-state convoy.
    # uhubctl exits 0 even when it does nothing ("No compatible devices detected" still returns
    # 0), so its status proves nothing -- the sysfs_gen check below is the only real verdict.
    [[ "$target" =~ $USBPATH_RE ]] || die "bad usb path: $target"
    UHUBCTL=$(command -v uhubctl || echo /sbin/uhubctl)
    [ -x "$UHUBCTL" ] || die "uhubctl not installed"
    # Existence alone only proves *something* occupies that path -- bus numbers renumber every
    # boot, so a stale busport can name a different device entirely and we would cut power to its
    # whole subtree (up to 25 fixtures on this rig). Callers that know what they expect pass the
    # serial as a third argument and we refuse on mismatch; otherwise print the identity so a
    # wrong target is at least visible.
    [ -e "/sys/bus/usb/devices/$target" ] || die "no such usb device: $target"
    idf="/sys/bus/usb/devices/$target"
    serial=$(lock_read "$idf/serial")
    expect=${3:-}
    if [ -n "$expect" ] && [ "$serial" = '?' ]; then
      # Warn and PROCEED: an unreadable serial is the wedge signature itself, so refusing
      # here would block the cure on exactly the condition it exists for. The identity
      # guard is lost for this call -- say so, because the cost of a wrong target is the
      # whole subtree.
      echo "root-cycle: WARNING $target's serial did not answer (it is wedged), so '$expect'" \
           "could NOT be confirmed; proceeding, but verify the busport if siblings drop" >&2
    elif [ -n "$expect" ] && [ "$expect" != "$serial" ]; then
      die "root-cycle: $target has serial '$serial', expected '$expect' — stale busport, refusing"
    fi
    # idVendor/idProduct are usb_descriptor_attr_le16: served WITHOUT the device lock, so
    # a plain cat is safe on a wedged device. serial/product are usb_string_attr and are not.
    echo "root-cycle: target $target is $(cat "$idf/idVendor" 2>/dev/null || echo -):$(cat "$idf/idProduct" 2>/dev/null || echo -)" \
         "serial=$serial product=$(lock_read "$idf/product")"
    bus=${target%%-*}; rest=${target#*-}; rootport=${rest%%.*}
    gen=$(sysfs_gen "$target")
    echo "root-cycle: disabling D+/D- on bus $bus root port $rootport (no VBUS cut; feeds $target, bounces its siblings)"
    # -S is load-bearing. By default uhubctl writes /sys/.../usb<bus>-port<n>/disable (observed:
    # two O_WRONLY opens per cycle), and the kernel's disable_store() takes the ROOT HUB's lock and
    # synchronously usb_disconnect()s the child BEFORE cutting power -- confirmed in v6.12.96
    # drivers/usb/core/port.c: usb_lock_device(hdev), the UNINTERRUPTIBLE variant, then
    # usb_disconnect(&port_dev->child) inside it. Against a wedged device that disconnect blocks on
    # the very lock we are trying to free, so power never drops and uhubctl D-states holding the
    # root hub's lock, poisoning the whole bus. -S forces the libusb path, which sends the
    # power-off control transfer straight to the root hub with no child-disconnect in front.
    "$UHUBCTL" -S -l "$bus" -p "$rootport" -a cycle -d 5 \
      || die "uhubctl failed to cycle bus $bus port $rootport"
    for _ in $(seq 1 10); do
      sleep 1
      now=$(sysfs_gen "$target")
      if [ "$now" != none ] && [ "$now" != "$gen" ]; then
        echo "root-cycled $bus port $rootport: $target re-enumerated"\
             "(devnum $(cat "/sys/bus/usb/devices/$target/devnum" 2>/dev/null || echo ?), gen $gen -> $now)"; exit 0
      fi
    done
    die "root-cycle: $target did not re-enumerate after cycling bus $bus port $rootport (sysfs generation still $gen: no disconnect happened)"
    ;;
  *)
    usage
    ;;
esac
