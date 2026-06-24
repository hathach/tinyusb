#!/usr/bin/env bash
# usbcap.sh - capture a USB bus via usbmon into a Wireshark-readable pcapng.
# Usage: usbcap.sh <bus|VID:PID|auto> [seconds] [outfile]
#   <bus>      numeric USB bus (lsusb "Bus 00N" -> N); 0 or "auto" = all buses.
#   <VID:PID>  resolve the bus from a plugged-in device, e.g. cafe: or 1a86:8010.
# Assumes usbmon is loaded and /dev/usbmon* is readable by your wireshark group
# (see SKILL.md), so tshark captures with no sudo.
set -euo pipefail

target="${1:?usage: usbcap.sh <bus|VID:PID|auto> [seconds] [outfile]}"
dur="${2:-10}"
out="${3:-/tmp/usbcap-$$.pcapng}"
[[ "$dur" =~ ^[0-9]+$ ]] || { echo "seconds must be an integer" >&2; exit 2; }

case "$target" in
  auto) bus=0 ;;
  *:*)  line=$(lsusb | grep -iF "ID ${target}" | head -1) || true
        bus=$(printf '%s' "$line" | sed -E 's/^Bus 0*([0-9]+).*/\1/')
        [ -n "$bus" ] || { echo "no device matching '$target' (plugged in?)" >&2; exit 1; } ;;
  *)    bus="$target" ;;
esac
[[ "$bus" =~ ^[0-9]+$ ]] || { echo "bad bus '$target'" >&2; exit 2; }

echo "capturing usbmon${bus} for ${dur}s -> $out"
tshark -i "usbmon${bus}" -a "duration:${dur}" -w "$out"
echo "saved $out  ($(tshark -r "$out" 2>/dev/null | wc -l | tr -d ' ') packets)"
echo "analyze: tshark -r $out   |   tshark -r $out -V"
