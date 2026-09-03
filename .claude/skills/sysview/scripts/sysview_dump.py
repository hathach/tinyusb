#!/usr/bin/env python3
"""Post-mortem SystemView dump: recover the last seconds of scheduling
history from a halted target, no live capture running.

Post-mortem mode (SEGGER_SYSVIEW_POST_MORTEM_MODE=1, see hw/bsp/
family_support.cmake's SYSVIEW_POST_MORTEM option, set via
-DSYSVIEW=<level> -DSYSVIEW_POST_MORTEM=1) makes the target write
its RTT "SysView" up-buffer as a plain overwrite ring: no host draining it,
the buffer always holds whatever fits of the most recent events. This
script attaches to an ALREADY-RUNNING (or already-crashed/wedged) target
over J-Link, halts it (that halt IS the autopsy point — do not reset first,
resetting or reflashing destroys the evidence), reads the RTT control
block's channel-1 ("SysView") ring descriptor, dumps the raw ring bytes,
and linearizes them oldest-to-newest into DIR/capture.SVDat — decodable
with `sysview_record.py --from-raw DIR/capture.SVDat --out DIR2`.

Linearization (WrOff-split, hardware-validated on same54_xplained — see
the SKILL for the A/B rationale): WrOff is the next byte the target will
write, so it also marks the OLDEST byte still valid in the ring —
ring[WrOff:] + ring[:WrOff] reorders the whole buffer oldest-to-newest.
The candidate built from RdOff instead (ring[WrOff:] + ring[:RdOff]) was
tried and rejected: SEGGER_RTT_WriteWithOverwriteNoLock's own bookkeeping
keeps RdOff == WrOff+1 once the ring has wrapped (so that candidate is
just this one plus one harmless duplicate byte) but leaves RdOff at 0
forever before the first wrap (RdOff is a host read cursor — nothing on
the target ever advances it otherwise) — which would discard all the
real just-written data in exactly the case a fast crash most needs it.

Leaves the core halted by default (you are mid-autopsy); --resume sends a
plain "g" so the target carries on.
"""
import argparse
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# Same-dir helpers, not re-implemented: resolve_probe() and rtt_cb_from_elf() (the latter reads
# the ELF symbol table directly, no arm-none-eabi-nm shell-out -- see its docstring there).
sys.path.insert(0, str(Path(__file__).resolve().parent))
from sysview_record import resolve_probe, rtt_cb_from_elf

RTT_MAGIC = b"SEGGER RTT"
# SEGGER_RTT_CB layout (see lib/SEGGER_RTT/RTT/SEGGER_RTT.h):
#   char acID[16]; int MaxNumUpBuffers; int MaxNumDownBuffers; SEGGER_RTT_BUFFER_UP aUp[...];
# aUp[] starts at +0x18; each SEGGER_RTT_BUFFER_UP is 6 words (24 bytes):
#   sName, pBuffer, SizeOfBuffer, WrOff, RdOff, Flags
# Channel 0 is always "Terminal" (SEGGER_RTT's own default up-buffer);
# SystemView's Init allocates the next free channel for "SysView", which is
# channel 1 as long as nothing else grabs an up-buffer first — confirmed on
# same54_xplained hardware (both a live SYSVIEW=4 and a POST_MORTEM=1 build).
AUP_OFFSET = 0x18
AUP_STRIDE = 0x18
SYSVIEW_CHANNEL = 1
HEADER_LEN = AUP_OFFSET + (SYSVIEW_CHANNEL + 1) * AUP_STRIDE  # magic..aUp[1] end


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def jlink(args, serial, cmds):
    """Run one attach-only JLinkExe session (no reset — never disturb a
    halted/wedged target). cmds is the command-file body, without -CommandFile's
    trailing 'q' (added here)."""
    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False) as f:
        f.write("\n".join([*cmds, "q\n"]))
        cmd_file = f.name
    try:
        r = run(["JLinkExe", "-USB", serial, "-device", args.device, "-if", "SWD",
                 "-speed", str(args.speed), "-autoconnect", "1", "-nogui", "1",
                 "-CommandFile", cmd_file], timeout=60)
        if r.returncode != 0:
            sys.exit(f"error: JLinkExe failed:\n{r.stdout}\n{r.stderr}")
        return r.stdout
    finally:
        Path(cmd_file).unlink(missing_ok=True)


def dump(args, serial, cb):
    tmp = Path(tempfile.mkdtemp(prefix="sysview_dump_"))
    try:
        # 1. Halt (no reset) and read the RTT control block header, far enough
        # to cover the magic + channel-1 ("SysView") ring descriptor.
        header_bin = tmp / "header.bin"
        jlink(args, serial, ["h", f"savebin {header_bin}, {cb}, 0x{HEADER_LEN:X}"])
        header = header_bin.read_bytes()
        if len(header) != HEADER_LEN:
            sys.exit(f"error: read {len(header)} bytes, expected {HEADER_LEN} "
                     f"— probe/target read failed")
        if header[:len(RTT_MAGIC)] != RTT_MAGIC:
            sys.exit(f"error: no 'SEGGER RTT' magic at {cb} — wrong ELF, or "
                     f"target RAM not yet initialized. Core left halted.")
        up1_off = AUP_OFFSET + SYSVIEW_CHANNEL * AUP_STRIDE
        _sname, p_buffer, size_of_buffer, wr_off, rd_off, _flags = \
            struct.unpack_from("<6I", header, up1_off)
        if size_of_buffer == 0 or wr_off >= size_of_buffer or rd_off >= size_of_buffer:
            sys.exit(f"error: implausible channel-{SYSVIEW_CHANNEL} descriptor "
                     f"(pBuffer=0x{p_buffer:X} size={size_of_buffer} "
                     f"WrOff={wr_off} RdOff={rd_off}) — not a SystemView build? "
                     f"Core left halted.")

        # 2. Halt (no-op, already halted) and dump the ring bytes.
        ring_bin = tmp / "ring.bin"
        cmds = ["h", f"savebin {ring_bin}, 0x{p_buffer:X}, 0x{size_of_buffer:X}"]
        if args.resume:
            cmds.append("g")
        jlink(args, serial, cmds)
        ring = ring_bin.read_bytes()
        if len(ring) != size_of_buffer:
            sys.exit(f"error: read {len(ring)} ring bytes, expected {size_of_buffer}")

        # 3. Linearize (WrOff-split — see module docstring) and write out.
        out = Path(args.out)
        out.mkdir(parents=True, exist_ok=True)
        linearized = ring[wr_off:] + ring[:wr_off]
        (out / "capture.SVDat").write_bytes(linearized)

        print(f"SizeOfBuffer={size_of_buffer} WrOff={wr_off} RdOff={rd_off}")
        print(f"wrote: {out / 'capture.SVDat'}")
        print(f"core left {'running (--resume)' if args.resume else 'halted'}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", required=True, help="J-Link device name, e.g. ATSAME54P20")
    ap.add_argument("--probe", required=True, help="J-Link serial or USB nickname")
    ap.add_argument("--elf", help="post-mortem-build ELF — _SEGGER_RTT address read from it")
    ap.add_argument("--rttcbaddr", help="RTT control block address (overrides --elf)")
    ap.add_argument("--out", required=True, help="output directory for capture.SVDat")
    ap.add_argument("--speed", type=int, default=4000, help="SWD speed kHz")
    ap.add_argument("--resume", action="store_true",
                    help="resume the core (g) after dumping; default leaves it halted")
    args = ap.parse_args()

    if not args.rttcbaddr and not args.elf:
        ap.error("need --elf or --rttcbaddr")
    for tool in ("JLinkExe",):
        if not shutil.which(tool):
            sys.exit(f"error: '{tool}' not on PATH")

    serial = resolve_probe(args.probe)
    cb = args.rttcbaddr or rtt_cb_from_elf(args.elf)
    dump(args, serial, cb)


if __name__ == "__main__":
    main()
