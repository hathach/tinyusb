#!/usr/bin/env python3
"""Unattended SEGGER SystemView profiling on a headless host.

Drives the SystemView GUI's live J-Link recorder under a private Xvfb: it
connects to the target over the debug probe, records SystemView events for a
window while an optional workload runs, then exports the analysis to CSV — no
desktop, no human clicks. What makes it work headlessly:

  * the per-launch SFL license dialog is dismissed by clicking "Continue under
    SFL" (the second button from the right — the corner is "Decline", which
    quits the app);
  * a fresh ini (the old one is removed, never merged — configparser corrupts
    its binary window-state blobs and SystemView then crashes on startup) so
    -start pops the recorder config dialog PREFILLED from the CLI args, whose
    Finish button is what actually connects J-Link;
  * a stale Xvfb on the target display is killed first (a leftover one makes
    our Xvfb fail to bind, leaving SystemView with no display).

The target must run a SystemView-instrumented firmware (build with
-DSYSVIEW=<level>, see hw/bsp/family_support.cmake) — tusb_sysview_init()
calls SEGGER_SYSVIEW_Start() automatically, no source edits needed. Outputs
in --out:
contexts.csv (per-task/ISR CPU load + run-time stats), recording.SVDat (raw,
openable in a desktop GUI), optionally events.txt / terminal.csv, and
debug.png on failure. Run on the host that owns the probe; one probe, one
client — no other J-Link tool may use it during recording.
"""
import argparse
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

SV_PORT = 19050
INI = Path.home() / ".config/SEGGER/SEGGER SystemView.ini"
INI_BAK = INI.with_name(INI.name + ".tinyusb-bak")


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def resolve_probe(probe):
    """Map a J-Link USB nickname to its serial via the JLinkExe banner
    (SystemView's -usb wants a serial when several probes are attached)."""
    if not probe or probe.isdigit():
        return probe
    r = run(["JLinkExe", "-USB", probe, "-nogui", "1"], input="qc\n", timeout=30)
    m = re.search(r"S/N:\s*(\d+)", r.stdout)
    if not m:
        sys.exit(f"error: cannot resolve probe '{probe}' to a serial")
    return m.group(1)


def rtt_cb_from_elf(elf):
    """Address of _SEGGER_RTT, read straight out of the ELF symbol table.

    Deliberately does not shell out to <prefix>nm: the prefix differs per target
    (arm-none-eabi- vs riscv-none-elf-) and neither is on PATH by default on the
    rig, which turned a working capture into a FileNotFoundError traceback.
    Every TinyUSB target is 32-bit little-endian, so only ELF32 LSB is handled.
    """
    with open(elf, "rb") as f:
        b = f.read()
    if b[:4] != b"\x7fELF" or b[4] != 1 or b[5] != 1:
        sys.exit(f"error: {elf} is not a 32-bit little-endian ELF")
    u16 = lambda o: int.from_bytes(b[o:o + 2], "little")
    u32 = lambda o: int.from_bytes(b[o:o + 4], "little")
    shoff, shentsize, shnum = u32(0x20), u16(0x2E), u16(0x30)
    for i in range(shnum):
        sh = shoff + i * shentsize
        if u32(sh + 4) != 2:  # SHT_SYMTAB
            continue
        symoff, symsize, entsize = u32(sh + 0x10), u32(sh + 0x14), u32(sh + 0x24)
        strtab = shoff + u32(sh + 0x18) * shentsize  # sh_link -> .strtab
        stroff = u32(strtab + 0x10)
        for s in range(symoff, symoff + symsize, entsize):
            name_off = stroff + u32(s)
            end = b.index(b"\0", name_off)
            if b[name_off:end] == b"_SEGGER_RTT":
                return hex(u32(s + 4))
    sys.exit(f"error: no _SEGGER_RTT symbol in {elf} — not a SystemView build?")


# ---------------------------------------------------------------- X helpers

def xdo(display, *args):
    return run(["xdotool", *args], env={**os.environ, "DISPLAY": display})


def visible_windows(display):
    wins = []
    for wid in xdo(display, "search", "--onlyvisible", "--name", ".").stdout.split():
        name = xdo(display, "getwindowname", wid).stdout.strip()
        g = xdo(display, "getwindowgeometry", "--shell", wid).stdout
        geo = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", g)}
        wins.append((wid, name, geo))
    return wins


def click(display, x, y):
    xdo(display, "mousemove", str(x), str(y), "click", "1")
    time.sleep(1.5)


def click_dialog(display, name_match, dx=45, timeout=20):
    """Click a bottom-right button of the first dialog whose title matches.
    dx = pixels from the right edge: 45 is the corner OK; 130 is the config
    dialog's Finish (corner is Cancel); 177 is the license 'Continue under
    SFL' (corner is Decline)."""
    t0 = time.time()
    while time.time() - t0 < timeout:
        for wid, name, g in visible_windows(display):
            if name_match(name):
                click(display, g["X"] + g["WIDTH"] - dx, g["Y"] + g["HEIGHT"] - 25)
                return True
        time.sleep(1)
    return False


def dismiss_dialogs(display, rounds=6):
    """Click the bottom-right button of every small (dialog-sized) window,
    a few rounds. Used after -stop where the modal titles vary ('SystemView
    overflow events recorded' with a Close button, info popups) — matching by
    size, not title, is what reliably clears them so -save is not blocked."""
    for _ in range(rounds):
        hit = False
        for wid, name, g in visible_windows(display):
            if g["WIDTH"] < 900 and g["HEIGHT"] < 720:  # a dialog, not the main window
                click(display, g["X"] + g["WIDTH"] - 45, g["Y"] + g["HEIGHT"] - 25)
                hit = True
        if not hit:
            return
        time.sleep(1)


def free_display(display):
    run(["pkill", "-9", "-f", f"Xvfb {display} "])
    time.sleep(1)


# ---------------------------------------------------------------- socket

def sv_cmd(cmd, timeout=10):
    with socket.create_connection(("127.0.0.1", SV_PORT), timeout=timeout) as s:
        s.sendall((cmd + "\n").encode())
        time.sleep(0.3)


def wait_port(port, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            socket.create_connection(("127.0.0.1", port), timeout=1).close()
            return True
        except OSError:
            time.sleep(1)
    return False


def wait_file(path, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if path.exists() and path.stat().st_size > 0:
            return True
        time.sleep(1)
    return False


def _is_our_stub(path):
    """True if the file is the minimal ini this script writes, not a real SystemView config."""
    try:
        t = path.read_text()
    except OSError:
        return False
    return t.startswith("[Preferences]") and len(t) < 200 and "LoadProjectOnStart=false" in t


def stash_ini():
    """Overwrite the SystemView ini with our minimal stub, first backing up
    whatever was there (a developer's window layout, recent-projects list,
    recorder presets) so restore_ini()'s finally-block call gives it back --
    without this, record()/decode_raw() previously clobbered it permanently.
    Skips taking a new backup if INI_BAK already exists: that means an earlier
    run crashed before restoring, and INI_BAK still holds the real original
    (copying again here would overwrite it with our own stub instead).
    License acknowledgement lives in a separate file
    (SEGGER_REG_HKEY_CURRENT_USER.xml) -- never touched by this script."""
    INI.parent.mkdir(parents=True, exist_ok=True)
    # Never back up our OWN stub as if it were the user's config. If a run is SIGKILLed the
    # finally-block restore never happens, leaving the stub in place with its backup already
    # consumed; the next run would then "back up" the stub and the real config is gone for good.
    # Observed on ci.lan: a 19 KB SystemView config replaced by the 60-byte stub, unrecoverable.
    if INI.exists() and not INI_BAK.exists() and not _is_our_stub(INI):
        shutil.copy2(INI, INI_BAK)
    INI.write_text("[Preferences]\n"
                   "LoadProjectOnStart=false\n"
                   "SaveProperties=false\n")


def check_not_sample(out):
    """Fail loudly if the export is SEGGER's bundled demo rather than our target.

    A wrong-data export decodes cleanly and reads plausibly, so nothing downstream
    can tell it apart -- the only cheap discriminator is the context names, which
    on the shipped LPC4367/embOS demos are nothing like a TinyUSB build's. Checked
    by name rather than by count so an idle-but-real capture still passes."""
    ctx = out / "contexts.csv"
    if not ctx.exists():
        return
    names = ctx.read_text(errors="replace")
    demo = [m for m in ("Job Runner", "Compass", "Acceleration", "M4CORE", "M0APP")
            if m in names]
    if demo:
        sys.exit(f"error: export contains SEGGER's bundled demo recording "
                 f"(saw {', '.join(demo)}), not this target's trace. SystemView "
                 f"auto-loaded its last data file; ensure LoadDataOnStart=false "
                 f"reached the ini, and re-record.")


def restore_ini():
    """Undo stash_ini(): restore the backed-up ini, or remove our stub if
    there was nothing to restore (no ini existed before this run)."""
    if INI_BAK.exists():
        shutil.move(str(INI_BAK), str(INI))
    else:
        INI.unlink(missing_ok=True)


def clear_stale_exports(paths):
    """Delete any pre-existing files at these paths before a fresh export run.
    wait_file() above only checks exists()+size>0 -- a stale file left over from a
    previous invocation (e.g. a prior run that got this far and no further) would
    let a silently-failed -export/-save this run pass as if it had produced fresh
    output."""
    for p in paths:
        try:
            p.unlink()
        except FileNotFoundError:
            pass


# ---------------------------------------------------------------- main flow

def record(args, serial, rtt):
    out = Path(args.out)
    disp = args.display
    exports = [("-save", out / "recording.SVDat"),
               ("-export-contexts", out / "contexts.csv")]
    if not args.no_events:
        exports.append(("-export", out / "events.txt"))
    if args.export_terminal:
        exports.append(("-export-terminal", out / "terminal.csv"))
    # Before launching SystemView: a stale export left over from a previous invocation
    # would let wait_file() below pass a silently-failed -save/-export this run as fresh.
    clear_stale_exports(p for _, p in exports)
    # Fresh MINIMAL ini: overwrite (never merge — configparser corrupts the
    # binary @ByteArray window-state blobs and SystemView crashes on startup).
    # SaveProperties=false stops the Recording-Properties dialog blocking -save.
    # Do NOT set LoadDataOnStart=false: the auto-loaded startup recording is
    # what makes -start pop the recorder config dialog (whose Finish connects
    # J-Link) — suppress it and -start silently records nothing. Its
    # "Events loaded" modal is dismissed below before the socket wait.
    # stash_ini()/restore_ini() (below, in the finally block) back this up and
    # give it back so a developer's window layout etc. survives the run.
    stash_ini()
    free_display(disp)
    xvfb = subprocess.Popen(["Xvfb", disp, "-screen", "0", "1600x1000x24"],
                            stderr=subprocess.DEVNULL)
    sv = None
    traffic = None
    try:
        time.sleep(1)
        sv = subprocess.Popen(
            ["systemview", "-single", "-recorder", "J-Link", "-device", args.device,
             "-usb", serial, "-if", "SWD", "-speed", str(args.speed), "-rttcbaddr", rtt],
            env={**os.environ, "DISPLAY": disp},
            stdout=open(out / "systemview.log", "w"), stderr=subprocess.STDOUT)

        click_dialog(disp, lambda n: "License" in n or "Commercial" in n, dx=177, timeout=30)
        # a sample recording auto-loads on a fresh ini — dismiss its info modal
        click_dialog(disp, lambda n: "Events loaded" in n or "System Information" in n, timeout=8)
        if not wait_port(SV_PORT, 30):
            raise RuntimeError("SystemView command server (:19050) never came up "
                               "— license dialog not dismissed? see debug.png")

        sv_cmd("-start")
        # -start pops the recorder config dialogs (prefilled from CLI args):
        # a small recorder-type picker ("SystemView Recorder:" dropdown,
        # OK/Cancel) then the large "Recorder Configuration" J-Link config
        # dialog (Finish/Cancel). On BOTH, the confirm button (OK / Finish)
        # sits at dx=130 from the right edge — Cancel is the corner button
        # (dx=45) on both, not OK, despite the smaller dialog's title being
        # just "Recorder Configuration" with no size cue otherwise.
        #
        # A probe running ST-Link-compatible J-Link firmware (e.g. an onboard
        # ST-Link reflashed with J-Link firmware, as used on some Discovery
        # boards) additionally pops a one-time-per-day "Terms of use" dialog
        # AFTER Finish, asynchronously (a few seconds later, once the J-Link
        # DLL actually opens the probe) — Accept is the corner button here
        # (dx=45), Decline is second-from-right (dx=134): the opposite
        # corner convention from the license dialog. Left unhandled, the
        # connect silently stalls and the export step below re-exports
        # whatever was already loaded (e.g. SystemView's bundled sample
        # recording on a fresh ini) instead of erroring — verified on ci.lan.
        #
        # Poll the WHOLE window — a dialog can appear a few seconds after
        # -start (or after the previous one is dismissed), so require two
        # consecutive empty polls (not just one) before concluding no more
        # dialogs are coming.
        seen_config = False
        misses = 0
        t0 = time.time()
        while time.time() - t0 < 30:
            hit = False
            for wid, name, g in visible_windows(disp):
                if "Terms of use" in name:
                    click(disp, g["X"] + g["WIDTH"] - 45, g["Y"] + g["HEIGHT"] - 25)
                    hit = seen_config = True
                elif "Recorder" in name or "Connection" in name or "Configuration" in name:
                    click(disp, g["X"] + g["WIDTH"] - 130, g["Y"] + g["HEIGHT"] - 25)
                    hit = seen_config = True
            if seen_config and not hit:
                misses += 1
                if misses >= 2:
                    break
            else:
                misses = 0
            time.sleep(1)
        time.sleep(3)  # J-Link connect + first events

        # start_new_session=True: traffic_cmd runs under `sh -c`, which can itself spawn
        # children (e.g. a pipeline) -- putting it in its own process group lets the
        # finally block below kill the whole group, not just the shell.
        traffic = subprocess.Popen(args.traffic_cmd, shell=True, start_new_session=True) \
            if args.traffic_cmd else None
        time.sleep(args.duration_ms / 1000)
        if traffic:
            try:
                traffic.wait(timeout=60)
            except subprocess.TimeoutExpired:
                pass  # killed in the finally block below, whole process group

        sv_cmd("-stop")
        time.sleep(2)
        # After stop an "overflow events recorded" / info modal (with a Close
        # button) can block -save. Its title varies, so clear by size.
        dismiss_dialogs(disp)

        for cmd, path in exports:
            sv_cmd(f"{cmd} {path}")
            time.sleep(1)
            dismiss_dialogs(disp, rounds=3)  # clear any save/confirm modal
            if not wait_file(path, 60):
                raise RuntimeError(f"{cmd} produced no file — see debug.png")
        sv_cmd("-quit")
        try:
            sv.wait(timeout=15)
        except subprocess.TimeoutExpired:
            pass
    except Exception:
        run(["import", "-window", "root", str(out / "debug.png")],
            env={**os.environ, "DISPLAY": disp})
        raise
    finally:
        if traffic and traffic.poll() is None:
            try:
                os.killpg(os.getpgid(traffic.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
        if sv and sv.poll() is None:
            sv.kill()
        xvfb.kill()
        restore_ini()


def decode_raw(args):
    """Load a raw capture (e.g. sysview_dump.py's post-mortem capture.SVDat)
    and export it — no probe, no J-Link, no --device/--probe needed. Same
    Xvfb/dialog choreography as record(), minus the live-recorder connect."""
    out = Path(args.out)
    disp = args.display
    exports = [("-export-contexts", out / "contexts.csv")]
    if not args.no_events:
        exports.append(("-export", out / "events.txt"))
    if args.export_terminal:
        exports.append(("-export-terminal", out / "terminal.csv"))
    # Before launching SystemView: a stale export left over from a previous invocation
    # would let wait_file() below pass a silently-failed -export this run as fresh. Does
    # not touch args.from_raw (the source capture being decoded, a different path).
    clear_stale_exports(p for _, p in exports)
    stash_ini()
    free_display(disp)
    xvfb = subprocess.Popen(["Xvfb", disp, "-screen", "0", "1600x1000x24"],
                            stderr=subprocess.DEVNULL)
    sv = None
    try:
        time.sleep(1)
        sv = subprocess.Popen(
            ["systemview", "-single", "-port", str(SV_PORT), "-wait"],
            env={**os.environ, "DISPLAY": disp},
            stdout=open(out / "systemview.log", "w"), stderr=subprocess.STDOUT)

        click_dialog(disp, lambda n: "License" in n or "Commercial" in n, dx=177, timeout=30)
        # Same fresh-ini auto-load info modal record() dismisses (symmetry — untested
        # whether this launch mode, no -recorder args, can actually trigger it, but
        # dismiss_dialogs() below is a no-op if nothing is there).
        click_dialog(disp, lambda n: "Events loaded" in n or "System Information" in n, timeout=8)
        if not wait_port(SV_PORT, 30):
            raise RuntimeError("SystemView command server (:19050) never came up "
                               "— license dialog not dismissed? see debug.png")

        sv_cmd(f"-load {Path(args.from_raw).resolve()}")
        time.sleep(2)
        dismiss_dialogs(disp)  # "System Information" popup(s) after a raw load

        for cmd, path in exports:
            sv_cmd(f"{cmd} {path}")
            time.sleep(1)
            dismiss_dialogs(disp, rounds=3)
            if not wait_file(path, 60):
                raise RuntimeError(f"{cmd} produced no file — see debug.png")
        sv_cmd("-quit")
        try:
            sv.wait(timeout=15)
        except subprocess.TimeoutExpired:
            pass
    except Exception:
        run(["import", "-window", "root", str(out / "debug.png")],
            env={**os.environ, "DISPLAY": disp})
        raise
    finally:
        if sv and sv.poll() is None:
            sv.kill()
        xvfb.kill()
        restore_ini()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", help="J-Link device name, e.g. ATSAME54P20 (live capture)")
    ap.add_argument("--probe", help="J-Link serial or USB nickname (live capture)")
    ap.add_argument("--elf", help="instrumented ELF — _SEGGER_RTT address read from it")
    ap.add_argument("--rttcbaddr", help="RTT control block address (overrides --elf)")
    ap.add_argument("--duration-ms", type=int, default=10000)
    ap.add_argument("--out", required=True, help="output directory")
    ap.add_argument("--speed", type=int, default=4000, help="SWD speed kHz")
    ap.add_argument("--display", default=":96", help="private Xvfb display")
    ap.add_argument("--traffic-cmd", help="shell command run during the recording window")
    ap.add_argument("--from-raw", help="decode a raw capture (e.g. sysview_dump.py's "
                    "capture.SVDat) instead of recording live — no --device/--probe needed")
    ap.add_argument("--no-events", action="store_true",
                    help="skip events.txt export (large: ~3 MB per second recorded)")
    ap.add_argument("--export-terminal", action="store_true",
                    help="export terminal.csv (SEGGER_SYSVIEW_Print* output)")
    args = ap.parse_args()

    if args.from_raw:
        if not Path(args.from_raw).is_file():
            ap.error(f"--from-raw {args.from_raw} not found")
    else:
        if not args.rttcbaddr and not args.elf:
            ap.error("need --elf or --rttcbaddr")
        if not (args.device and args.probe):
            ap.error("need --device and --probe for live capture")
    # SystemView deletes stale /tmp/sv-* dirs on startup (its own temp-dir
    # pattern) — an --out matching it gets wiped mid-run and -save has nowhere
    # to write. Reject it.
    op = Path(args.out).resolve()
    if op.parent == Path("/tmp") and op.name.startswith("sv-"):
        ap.error(f"--out {args.out} collides with SystemView's /tmp/sv-* temp dirs "
                 f"(it deletes them on startup) — use e.g. /tmp/sysview-<board>")
    tools = ["systemview", "Xvfb", "xdotool", "import"]
    if not args.from_raw:
        tools.append("JLinkExe")
    for tool in tools:
        if not shutil.which(tool):
            sys.exit(f"error: '{tool}' not on PATH (apt: systemview deb, xvfb, "
                     f"xdotool, imagemagick; SEGGER J-Link package)")

    Path(args.out).mkdir(parents=True, exist_ok=True)
    if args.from_raw:
        decode_raw(args)
    else:
        serial = resolve_probe(args.probe)
        rtt = args.rttcbaddr or rtt_cb_from_elf(args.elf)
        record(args, serial, rtt)
    check_not_sample(Path(args.out))
    print(f"recorded -> {Path(args.out) / 'contexts.csv'}")


if __name__ == "__main__":
    main()
