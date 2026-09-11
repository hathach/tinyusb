#!/usr/bin/env bash
# usb_dyndbg.sh — toggle kernel dynamic-debug on USB drivers (host or gadget
# side); run with sudo. Flips +p/-p only on an allowlisted set of USB modules,
# so it can't reach arbitrary kernel debug or unrelated subsystems.
set -euo pipefail

CTL=${USB_DYNDBG_CTL:-/sys/kernel/debug/dynamic_debug/control}
# Allowlist: USB core + host-controller + common class drivers, plus the
# gadget/UDC side of a Linux peer (dwc2/dwc3, udc_core, libcomposite).
ALLOW='usbcore xhci_hcd xhci_pci xhci_pci_renesas ehci_hcd ehci_pci ohci_hcd ohci_pci uhci_hcd dwc2 dwc3 cdc_acm usb_storage uas libcomposite udc_core'

die() { echo "usb_dyndbg: $*" >&2; exit 1; }
help() {
  cat <<EOF
usage: sudo usb_dyndbg.sh on  <module>...   enable the print flag (+p) at every site of each module
       sudo usb_dyndbg.sh off <module>...   disable it (-p); always do this when done, it is very noisy
       sudo usb_dyndbg.sh status [module]   sites with the print flag set, for one module or every allowlisted one

modules (host side, then a Linux gadget peer's device side):
  $ALLOW
Pick the host-controller module from \`lsusb -t\` (Driver=); usbcore covers enumeration and hub logic.
Needs CONFIG_DYNAMIC_DEBUG and a mounted debugfs ($CTL).
EOF
}
usage() { help >&2; exit 2; }
allowed() { local m; for m in $ALLOW; do [ "$m" = "$1" ] && return 0; done; return 1; }

# Control lines are: file:line [module]function =flags "format"; `p` in flags
# is the print flag, `=_` means none set.
sites() {  # sites <module|-> : lines of the allowlisted module(s) with p set; exit 3 if none, awk's status on a read error
  local module=$1
  awk -v module="$module" -v allow=" $ALLOW " '
    {
      m = $2; sub(/\].*/, "", m); sub(/^\[/, "", m)
      if (module != "-" ? m != module : index(allow, " " m " ") == 0) next
      if ($3 ~ /^=[a-z_]*p/) { print; found = 1 }
    }
    END { exit found ? 0 : 3 }' "$CTL"
}

action=${1:-}; shift || true
case "$action" in
  -h|--help|help) help; exit 0 ;;
  on|off|status) ;;
  *) usage ;;
esac
if [ ! -e "$CTL" ]; then
  [ -e "${CTL%/*/*}" ] && [ ! -r "${CTL%/*/*}" ] && die "cannot access ${CTL%/*/*} (run with sudo?)"
  die "dynamic_debug unavailable (need CONFIG_DYNAMIC_DEBUG + debugfs mounted at $CTL)"
fi

case "$action" in
  on|off)
    [ "$#" -ge 1 ] || usage
    flag='+p'; [ "$action" = off ] && flag='-p'
    for m in "$@"; do allowed "$m" || die "module not allowlisted: $m"; done
    for m in "$@"; do
      echo "module $m $flag" > "$CTL" || die "cannot write $CTL (run with sudo?)"
      echo "dynamic debug $action: $m"
    done
    ;;
  status)
    m=${1:--}
    [ "$m" = - ] || allowed "$m" || die "module not allowlisted: $m"
    [ -r "$CTL" ] || die "cannot read $CTL (run with sudo?)"
    rc=0; sites "$m" || rc=$?
    case "$rc" in
      0) ;;
      3) [ "$m" = - ] && echo "(no print sites enabled in any allowlisted module)" || echo "(no print sites enabled for $m)" ;;
      *) die "cannot read $CTL (awk exited $rc)" ;;
    esac
    ;;
esac
