#!/usr/bin/env bash
# usb_recover.sh — USB recovery helper for the HIL rig; run with sudo. Writes only
# to the specific sysfs control files below; arg regexes block path traversal.
#
# Usage:
#   sudo usb_recover.sh authorized <busport>   # e.g. 3-2  -> deauthorize+reauthorize (re-enumerate, NO VBUS cut)
#   sudo usb_recover.sh root-cycle <busport> [serial]  # e.g. 13-1.6 -> uhubctl port-off/on at the ROOT port feeding
#                                              # it; [serial] is verified against the device and refused on mismatch,
#                                              # skipping the leaf hubs (which fake ganged switching and do not
#                                              # actually cut power). Bounces every sibling under that root port.
#                                              # The D-state escape: no device lock, so it cannot convoy.
#   sudo usb_recover.sh pci-rebind <pciaddr>   # e.g. 0000:05:00.0 -> unbind+bind the whole xHCI
#                                              # controller. For a DEAD CONTROLLER, not a wedged
#                                              # device: it renumbers every bus it owns.
#   sudo usb_recover.sh pci-bind   <pciaddr> [drv]  # re-attach a driver to a DRIVERLESS controller
#   sudo usb_recover.sh resolve    <devnode>   # e.g. /dev/ttyACM3 -> print its <busport> (no privilege needed)
set -euo pipefail

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
usage() { grep -E '^#   sudo usb_recover' "$0" >&2; exit 2; }

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
[ -n "$action" ] && [ -n "$target" ] || usage

case "$action" in
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
