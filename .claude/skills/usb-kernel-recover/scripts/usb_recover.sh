#!/usr/bin/env bash
# usb_recover.sh — USB recovery helper for the HIL rig; run with sudo. Writes only
# to the specific sysfs control files below; arg regexes block path traversal.
#
# Usage:
#   sudo usb_recover.sh authorized <busport>   # e.g. 3-2  -> deauthorize+reauthorize (re-enumerate, NO VBUS cut)
#   sudo usb_recover.sh rebind     <busport>   # e.g. 3-2  -> usb driver unbind+bind (re-probe)
#   sudo usb_recover.sh pci-rebind <pciaddr>   # e.g. 0000:01:00.0 -> HCD unbind+bind (WHOLE controller)
#   sudo usb_recover.sh pci-bind   <pciaddr> [driver]  # bind a DRIVERLESS controller (e.g. after a pci-rebind
#                                              # whose re-bind hung and left it unbound). Auto-tries the xHCI
#                                              # drivers (xhci-pci-renesas, xhci_hcd) unless one is named.
#   sudo usb_recover.sh hub-cycle  <busport>   # e.g. 13-1.6 -> uhubctl power-cycle of the port feeding it,
#                                              # walking upstream (parent hub -> root port) until the device
#                                              # re-enumerates. Ganged/fake-switching hubs may bounce ALL
#                                              # siblings; self-powered hubs only reset their uplink, which
#                                              # is why the walk ends at the root port (real xHCI ppps).
#   sudo usb_recover.sh root-cycle <busport> [serial]  # e.g. 13-1.6 -> uhubctl VBUS cut at the ROOT port feeding
#                                              # it; [serial] is verified against the device and refused on mismatch,
#                                              # skipping the leaf hubs (which fake ganged switching and do not
#                                              # actually cut power). Bounces every sibling under that root port.
#                                              # The D-state escape: no device lock, so it cannot convoy.
#   sudo usb_recover.sh resolve    <devnode>   # e.g. /dev/ttyACM3 -> print its <busport> (no privilege needed)
set -euo pipefail

USBPATH_RE='^[0-9]+-[0-9]+(\.[0-9]+)*$'
PCI_RE='^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9]$'
DRIVER_RE='^[A-Za-z0-9_-]+$'

die() { echo "usb_recover: $*" >&2; exit 1; }

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
# recreated -- same inode -- which is exactly the case that must be reported as a failure.
sysfs_gen() { stat -c %i "/sys/bus/usb/devices/$1/" 2>/dev/null || echo none; }
usage() { grep -E '^#   sudo usb_recover' "$0" >&2; exit 2; }

# Refuse to touch a PCI function that is not a USB controller (class 0x0c03xx), so a stray or
# mistyped BDF can't unbind/reset an unrelated device (storage, NIC) on a shared HIL host.
require_usb_controller() {
  local addr=$1 cls
  cls=$(cat "/sys/bus/pci/devices/$addr/class" 2>/dev/null) || die "no such pci device: $addr"
  [[ "$cls" =~ ^0x0c03 ]] || die "$addr is not a USB controller (class $cls); refusing"
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
  rebind)
    [[ "$target" =~ $USBPATH_RE ]] || die "bad usb path: $target"
    [ -e "/sys/bus/usb/devices/$target" ] || die "no such usb device: $target"
    echo "$target" > /sys/bus/usb/drivers/usb/unbind; sleep 1
    echo "$target" > /sys/bus/usb/drivers/usb/bind
    echo "rebound $target"
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
  hub-cycle)
    [[ "$target" =~ $USBPATH_RE ]] || die "bad usb path: $target"
    UHUBCTL=$(command -v uhubctl || echo /sbin/uhubctl)
    [ -x "$UHUBCTL" ] || die "uhubctl not installed"
    # sysfs generation, not node existence: a disconnect blocked on the device lock leaves the
    # old node (and its idVendor) in place, so an existence check reports success without anything
    # having happened -- and the walk to the root port, which is the part that actually cuts power
    # on these fake-ganged leaf hubs, would never run.
    gen=$(sysfs_gen "$target")
    dev="$target"
    while :; do
      if [[ "$dev" =~ ^([0-9]+)-([0-9]+)$ ]]; then    # parent is the root hub
        loc="${BASH_REMATCH[1]}"; port="${BASH_REMATCH[2]}"; up=""
      else                                            # parent is a downstream hub
        loc="${dev%.*}"; port="${dev##*.}"; up="$loc"
      fi
      echo "hub-cycle: power-cycling hub $loc port $port (feeds $dev)"
      "$UHUBCTL" -l "$loc" -p "$port" -a cycle -d 5 -f || echo "  (uhubctl failed at $loc; walking up)"
      for _ in $(seq 1 10); do
        sleep 1
        now=$(sysfs_gen "$target")
        if [ "$now" != none ] && [ "$now" != "$gen" ]; then
          echo "recovered: $target re-enumerated (gen $gen -> $now)"; exit 0
        fi
      done
      [ -n "$up" ] || break
      dev="$up"
    done
    die "hub-cycle: $target still not enumerated after cycling up to the root port"
    ;;
  root-cycle)
    # VBUS cut at the ROOT port, where xHCI ppps is real. Unlike hub-cycle this does not walk up
    # from the leaf (the 1a40:0201 hubs claim ganged switching but never cut power) and never
    # writes the wedged device's sysfs or takes its lock, so it cannot join a D-state convoy.
    # uhubctl exits 0 even when it does nothing ("No compatible devices detected" still returns
    # 0), so its status proves nothing -- the devnum check below is the only real verdict.
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
    serial=$(cat "$idf/serial" 2>/dev/null || echo -)
    expect=${3:-}
    [ -z "$expect" ] || [ "$expect" = "$serial" ] || \
      die "root-cycle: $target has serial '$serial', expected '$expect' — stale busport, refusing"
    echo "root-cycle: target $target is $(cat "$idf/idVendor" 2>/dev/null):$(cat "$idf/idProduct" 2>/dev/null)" \
         "serial=$serial product=$(cat "$idf/product" 2>/dev/null || echo -)"
    bus=${target%%-*}; rest=${target#*-}; rootport=${rest%%.*}
    gen=$(sysfs_gen "$target")
    echo "root-cycle: cutting VBUS on bus $bus root port $rootport (feeds $target, bounces its siblings)"
    # -S is load-bearing. By default uhubctl writes /sys/.../usb<bus>-port<n>/disable (verified:
    # two O_WRONLY opens per cycle), and the kernel's disable_store() takes the ROOT HUB's lock and
    # synchronously usb_disconnect()s the child BEFORE cutting power -- against a wedged device that
    # blocks on the lock we are trying to free, so power would never drop and uhubctl would D-state
    # holding the root hub's lock, poisoning the whole bus. -S forces the libusb path, which sends
    # the power-off control transfer straight to the root hub with no child-disconnect in front.
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
