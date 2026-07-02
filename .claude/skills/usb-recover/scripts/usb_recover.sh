#!/usr/bin/env bash
# usb_recover.sh — narrow, sudoers-gated USB recovery helper for the HIL rig.
#
# Exposes exactly three sysfs-based reset actions, each with validated args, so
# it is safe to grant a single NOPASSWD sudoers rule for just this script
# instead of open root. It only ever writes to the specific sysfs control files
# below; the argument regexes prevent path traversal / arbitrary writes.
#
# Install (run once, needs your password):
#   sudo install -m0755 -oroot -groot usb_recover.sh /usr/local/sbin/usb_recover.sh
#   echo "$USER ALL=(root) NOPASSWD: /usr/local/sbin/usb_recover.sh" | sudo tee /etc/sudoers.d/usb-recover
#   sudo chmod 0440 /etc/sudoers.d/usb-recover && sudo visudo -c
#
# Usage:
#   sudo usb_recover.sh authorized <busport>   # e.g. 3-2  -> deauthorize+reauthorize (re-enumerate, NO VBUS cut)
#   sudo usb_recover.sh rebind     <busport>   # e.g. 3-2  -> usb driver unbind+bind (re-probe)
#   sudo usb_recover.sh pci-rebind <pciaddr>   # e.g. 0000:01:00.0 -> HCD unbind+bind (WHOLE controller)
#   sudo usb_recover.sh pci-reset  <pciaddr>   # e.g. 0000:01:00.0 -> PCI function-level reset: kills URBs at
#                                              # HW level WITHOUT the device lock; the only cure when a process
#                                              # is stuck in D state (usbfs ioctl) and unbind paths would convoy
#   sudo usb_recover.sh resolve    <devnode>   # e.g. /dev/ttyACM3 -> print its <busport> (no privilege needed)
set -euo pipefail

USBPATH_RE='^[0-9]+-[0-9]+(\.[0-9]+)*$'
PCI_RE='^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9]$'

die() { echo "usb_recover: $*" >&2; exit 1; }
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
  rebind)
    [[ "$target" =~ $USBPATH_RE ]] || die "bad usb path: $target"
    [ -e "/sys/bus/usb/devices/$target" ] || die "no such usb device: $target"
    echo "$target" > /sys/bus/usb/drivers/usb/unbind; sleep 1
    echo "$target" > /sys/bus/usb/drivers/usb/bind
    echo "rebound $target"
    ;;
  pci-rebind)
    [[ "$target" =~ $PCI_RE ]] || die "bad pci addr: $target"
    [ -e "/sys/bus/pci/devices/$target/driver" ] || die "no driver bound to $target"
    drv=$(basename "$(readlink -f "/sys/bus/pci/devices/$target/driver")")
    echo "$target" > "/sys/bus/pci/drivers/$drv/unbind"; sleep 1
    echo "$target" > "/sys/bus/pci/drivers/$drv/bind"
    echo "rebound pci $target ($drv)"
    ;;
  pci-reset)
    [[ "$target" =~ $PCI_RE ]] || die "bad pci addr: $target"
    [ -e "/sys/bus/pci/devices/$target/reset" ] || die "no reset support on $target"
    echo 1 > "/sys/bus/pci/devices/$target/reset"
    echo "flr-reset pci $target"
    ;;
  *)
    usage
    ;;
esac
