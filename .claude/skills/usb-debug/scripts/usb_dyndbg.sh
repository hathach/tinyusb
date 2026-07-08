#!/usr/bin/env bash
# usb_dyndbg.sh — narrow, sudoers-gated toggle for kernel dynamic-debug on USB
# host drivers. It only flips the print flag (+p / -p) on an allowlisted set of
# USB-related modules, so a NOPASSWD grant for just this script can't be abused
# to control arbitrary kernel debug or flood logs from unrelated subsystems.
#
# Usage:
#   sudo usb_dyndbg.sh on  <module>...   # enable  +p  (e.g. usbcore xhci_hcd)
#   sudo usb_dyndbg.sh off <module>...   # disable -p
#   sudo usb_dyndbg.sh status [module]   # show enabled sites (or one module's sites)
set -euo pipefail

CTL=/sys/kernel/debug/dynamic_debug/control
# Allowlist: USB host-controller + core + common host class drivers.
ALLOW='usbcore xhci_hcd xhci_pci xhci_pci_renesas ehci_hcd ehci_pci ohci_hcd ohci_pci uhci_hcd dwc2 cdc_acm usb_storage uas'

die() { echo "usb_dyndbg: $*" >&2; exit 1; }
usage() {
  echo "usage: usb_dyndbg.sh {on|off} <module>...   modules: $ALLOW" >&2
  echo "       usb_dyndbg.sh status [module]" >&2
  exit 2
}
allowed() { local m; for m in $ALLOW; do [ "$m" = "$1" ] && return 0; done; return 1; }

[ -e "$CTL" ] || die "dynamic_debug unavailable (need CONFIG_DYNAMIC_DEBUG + debugfs mounted)"

action=${1:-}; shift || true
case "$action" in
  on|off)
    [ "$#" -ge 1 ] || usage
    flag='+p'; [ "$action" = off ] && flag='-p'
    for m in "$@"; do allowed "$m" || die "module not allowlisted: $m"; done
    for m in "$@"; do echo "module $m $flag" > "$CTL"; echo "dynamic debug $action: $m"; done
    ;;
  status)
    m=${1:-}
    if [ -n "$m" ]; then
      allowed "$m" || die "module not allowlisted: $m"
      grep -E "\[$m\]" "$CTL" || echo "(no sites for $m)"
    else
      grep -E '=p( |$)' "$CTL" || echo "(no print sites enabled)"
    fi
    ;;
  *)
    usage
    ;;
esac
