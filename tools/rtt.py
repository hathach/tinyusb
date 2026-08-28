#!/usr/bin/env python3
"""RTT console/capture over a debug probe — importable classes + CLI (the rtt
skill's SKILL.md is the manual).

Three routes (see the skill's transport matrix for which route a probe gets).
--backend is always explicit:

  J-Link route (console/capture, channel 0 only)
      rtt.py --backend jlink --probe <sn> --device <JLINK_DEVICE> [--seconds N] [-i]
  OpenOCD route (native probes: ST-Link/CMSIS-DAP; console/capture, any channel)
      rtt.py --backend openocd [--probe <sn>] [--vid-pid "0xVVVV 0xPPPP"] \\
             --cfg "-f interface/stlink.cfg -f target/stm32h7x.cfg" \\
             (--elf <flashed.elf> | --addr 0x2000xxxx) [--channel N] [--seconds N] [-i]
             [--reset-before-attach]   # capture from the target's boot (SystemView)
  Post-mortem ring dump (J-Link, no halt — debug-AP reads)
      rtt.py --backend jlink --dump <out.bin> --probe <sn> --device <JLINK_DEVICE> \\
             (--elf <flashed.elf> | --addr 0x...)

The probe is owned for the whole run: flash and reset BEFORE starting this, never
reset the target while it is attached. Pin the probe: rigs and benches run
several (jlink: --probe serial; openocd: --probe and/or --vid-pid).

The classes (JlinkRtt for J-Link, OpenocdRtt for openocd-driven probes) expose
the slice of pyserial the HIL harness uses — read/in_waiting/write/close/timeout,
reset_input_buffer, context-manager use, plus an `eof` latch — and are imported
by test/hil/helper/hil_util.py, so this file is HARNESS-CRITICAL: a change here
is classified like a test/hil/ harness change (tools/ci_select.py) and runs the
console unit tests (pre-commit hil-test hook, test/hil/test/test_hil_rtt.py).
Stdlib only — hil_util imports this file, never the other way around.
"""
import argparse
import contextlib
import os
import re
import select
import shlex
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time


class RttError(RuntimeError):
    """Every way a console can break: stall, closed, dead or reset server.

    A RuntimeError subclass so existing `except RuntimeError` callers keep working,
    but named so the harness can tell a console failure from an unrelated
    NotImplementedError / 'dictionary changed size during iteration' and stop
    reporting harness bugs as board failures."""


def _pos_float_env(name: str, default: float) -> float:
    # mirrors hil_util.pos_float_env, including its rejection of inf/nan: an infinite
    # write timeout is an unbounded write, the very thing this knob exists to bound
    raw = os.environ.get(name)
    if raw is None:
        return default
    try:
        v = float(raw)
    except ValueError:
        print(f'warning: {name} is not a number; using {default}', file=sys.stderr, flush=True)
        return default
    if not (v > 0 and v < float('inf')):
        print(f'warning: {name}={v} is not usable; using {default}', file=sys.stderr, flush=True)
        return default
    return v


# whole-call deadline for write() — same env knob as the harness's serial twin
RTT_WRITE_TIMEOUT = _pos_float_env('HIL_SERIAL_WRITE_TIMEOUT', 10)

# J-Link Commander's telnet greeting, sent at connect BEFORE (or without) the control
# block being found: never target output. Three lines; the middle one is the PROBE
# MODEL string, which in libjlinkarm carries no 'SEGGER ' prefix (J-Link OH3,
# J-Trace H9, ...) though some builds do prefix it — match both shapes. Consumers
# judging "did the target speak" must strip these lines first.
RTT_BANNER_RE = re.compile(r'^(SEGGER J-|J-Link[ 0-9]|J-Trace[ 0-9]|Process:\s)')


def strip_banner(data: bytes, complete_only: bool = False) -> bytes:
    """Target bytes only: drop the J-Link server banner lines and blanks.

    Both harness consumers (hil_test's device_info verdict, hil_pool_check's
    aliveness score) must judge "did the target speak" through this one filter,
    or the same byte stream scores differently per consumer. complete_only=True
    additionally drops a trailing unterminated line — for poll loops judging a
    growing buffer, where a banner FRAGMENT at a read boundary (b'SEGG', b'Proce')
    would defeat the prefix regex and count as target output; the final verdict
    after the window should pass complete_only=False to keep a genuine
    unterminated tail."""
    lines = data.splitlines(keepends=False)
    if complete_only and data and not data.endswith((b'\n', b'\r')) and lines:
        lines = lines[:-1]
    return b'\n'.join(l for l in lines
                      if l.strip() and not RTT_BANNER_RE.match(l.decode('utf-8', errors='ignore')))


def free_ports(count: int) -> list:
    """Bind ephemeral ports and hand back the numbers. Boards run in parallel, so the
    RTT/GDB ports cannot be the SEGGER defaults or two boards collide.

    Known TOCTOU: the port is free when released here, but another process can claim
    it before the server binds it. Accepted — the server binds the port itself, so
    there is no fd to hand over. The post-connect re-poll catches the common outcome
    (our server lost the bind and died); a foreign listener that stays alive is not
    detectable here and would need the connected peer to be validated."""
    socks = []
    try:
        for _ in range(count):
            s = socket.socket()
            s.bind(('127.0.0.1', 0))
            socks.append(s)
        return [s.getsockname()[1] for s in socks]
    finally:
        for s in socks:
            s.close()


def nm_rtt_addr(elf: str, nm: str = None) -> int:
    """Control-block address from the FLASHED elf's symbol table. --addr is the way
    out when nm cannot read the file (another architecture, no toolchain)."""
    nm = nm or os.environ.get('RTT_NM', 'arm-none-eabi-nm')
    try:
        r = subprocess.run([nm, elf], capture_output=True, text=True, timeout=30)
    except FileNotFoundError:
        raise SystemExit(f'{nm} not on PATH — set RTT_NM=<your-nm>, or pass --addr')
    except subprocess.TimeoutExpired:
        raise SystemExit(f'{nm} did not finish reading {elf} in 30 s — pass --addr instead')
    if r.returncode != 0:
        raise SystemExit(f'{nm} could not read {elf}: {r.stderr.strip()[:200]}\n'
                         f'(wrong architecture? set RTT_NM=<your-nm>, or pass --addr)')
    for line in r.stdout.splitlines():
        # "<addr> <type> _SEGGER_RTT": a defined data symbol only — an undefined one
        # ("         U _SEGGER_RTT") has no address and would int('U', 16)
        m = re.match(r'^([0-9a-fA-F]+)\s+[bBdD]\s+_SEGGER_RTT$', line.strip())
        if m:
            return int(m.group(1), 16)
    raise SystemExit(f'no defined _SEGGER_RTT symbol in {elf} — was it built with LOGGER=rtt?')


class _SocketRtt:
    """Shared console core: a TCP socket onto an RTT server owned by self._proc.

    Subclasses build their server argv and call _spawn() + _connect() in __init__.
    One failure contract: RttError for every way the console can break (stall,
    closed, dead server) — callers are written for exactly it. A dead or resetting
    server LATCHES `eof` rather than raising from the read side, so read loops and
    the harness's `assert not ser.eof` triage see it without an exception racing
    them to a generic handler."""

    server = 'RTT server'   # for error messages

    def __init__(self, timeout: float = 0.1):
        self.timeout = timeout
        self._buf = b''
        self._eof = False
        self._sock = None
        self._proc = None
        self._log = None
        self._lock = threading.Lock()   # _buf is touched by the CLI pump thread too

    def _spawn(self, cmd: list, stdin=None) -> None:
        # server output spools to a temp file: a PIPE nobody drains blocks a
        # single-threaded server once 64 KiB of log accumulates (openocd at
        # polling_interval 1 against a resetting target fills that in minutes) and
        # the console goes silent with no error; the file also feeds _server_tail
        self._log = tempfile.NamedTemporaryFile(prefix='rtt-server-', suffix='.log')
        try:
            self._proc = subprocess.Popen(cmd, stdin=stdin, stdout=self._log,
                                          stderr=subprocess.STDOUT, start_new_session=True)
        except FileNotFoundError as e:
            self.close()
            raise RttError(f'RTT console: {e.filename or cmd[0]} not on PATH') from e
        except BaseException:
            # any other spawn failure (PermissionError...) must not leak the log fd
            self.close()
            raise

    def _connect(self, port: int) -> None:
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                try:
                    self._sock = socket.create_connection(('127.0.0.1', port), timeout=2)
                    break
                except OSError:
                    if self._proc.poll() is not None:
                        break
                    time.sleep(0.2)
            if self._sock is None:
                tail = self._server_tail()
                self.close()
                raise RttError(f'RTT console: {self.server} did not serve port {port}{tail}')
            if self._proc.poll() is not None:
                # the connect succeeded but our server is dead: a foreign process claimed
                # the port in the free_ports window — refuse a console wired to a stranger
                self.close()
                raise RttError(f'RTT console: {self.server} died after connect (port {port} hijacked?)')
            self._sock.setblocking(False)
        except (KeyboardInterrupt, SystemExit):
            # a signal mid-construction must not orphan the server we just spawned
            self.close()
            raise

    def _server_tail(self) -> str:
        log = getattr(self, '_log', None)
        if log:
            with contextlib.suppress(OSError, ValueError):
                with open(log.name, 'rb') as fh:
                    tail = fh.read()[-400:].decode(errors='replace')
                if tail:
                    return ' — ' + tail
        return ''

    def _drain(self) -> None:
        # LATCH, never raise: a peer reset or a socket closed under us ends the
        # stream exactly like an orderly EOF. Raising here raced the harness's
        # `assert not ser.eof` triage into a generic handler that re-flashes the
        # board, and leaked ConnectionResetError/ValueError to in_waiting callers.
        # the WHOLE body under the lock, not just the append: the CLI's -i pump thread
        # and the read loop drain the same socket concurrently, and recv->append being
        # non-atomic let chunks land out of order (measured: transposed 64-byte
        # segments in 3/6 stress trials)
        try:
            with self._lock:
                while self._sock and select.select([self._sock], [], [], 0)[0]:
                    try:
                        chunk = self._sock.recv(65536)
                    except (BlockingIOError, InterruptedError):
                        return
                    if not chunk:
                        self._eof = True
                        return
                    self._buf += chunk
        except (OSError, ValueError, TypeError, AttributeError):
            self._eof = True

    @property
    def eof(self) -> bool:
        """True once the server hung up AND everything it sent has been read out."""
        if self._sock is None:
            return True
        self._drain()
        return self._eof and not self._buf

    @property
    def in_waiting(self) -> int:
        if self._sock is None:
            # pyserial raises on a closed port; answering "N bytes waiting" from a
            # closed dead console would let a caller bug look like a healthy board
            raise RttError('RTT console is closed')
        self._drain()
        return len(self._buf)

    def read(self, size: int = 1) -> bytes:
        if size is None or size <= 0:
            # pyserial's read(0) returns b'' and consumes nothing; a negative size
            # must not silently hand over (or destroy) buffered bytes
            return b''
        if self._sock is None:
            raise RttError('RTT console is closed')
        self._drain()
        deadline = None if self.timeout is None else time.monotonic() + self.timeout
        while (len(self._buf) < size and not self._eof
               and (deadline is None or time.monotonic() < deadline)):
            time.sleep(0.005)
            self._drain()
        if self._eof and len(self._buf) < size:
            # dead server: pace the empty returns like a serial timeout would, so a
            # caller's read loop cannot busy-spin at 100% CPU (416k empty reads/s
            # measured unpaced). timeout=None deliberately diverges from pyserial's
            # block-forever: the eof latch makes "server is gone" knowable, and an
            # eternal block on it helps nobody -- paced empties + .eof is the contract.
            pace = self.timeout if self.timeout is not None else 0.1
            remaining = (deadline - time.monotonic()) if deadline is not None else pace
            time.sleep(max(0.0, min(remaining, pace)))
        with self._lock:
            out, self._buf = self._buf[:size], self._buf[size:]
        return out

    def reset_input_buffer(self) -> None:
        # pyserial surface: the host tests flush pre-reset backlog through this
        if self._sock is None:
            raise RttError('RTT console is closed')
        self._drain()
        with self._lock:
            self._buf = b''

    def write(self, data: bytes) -> int:
        # select+send, not sendall(): the socket is non-blocking for reads, and sendall()
        # on a non-blocking socket raises BlockingIOError as soon as the send buffer is
        # full, with no count of what already went out -- a caller cannot resume without
        # duplicating bytes. Same reason serial_write_all treats a short write as fatal.
        sock = self._sock   # snapshot: close() from another thread nulls the attribute
        if sock is None:
            raise RttError('RTT console is closed')
        self._drain()
        if self._eof:
            # TCP accepts exactly one send after peer death — without this the bytes
            # would "succeed" into the void and the read timeout gets blamed on the target
            raise RttError(f'RTT console write to a dead server ({self.server} gone)')
        sent = 0
        deadline = time.monotonic() + RTT_WRITE_TIMEOUT
        while sent < len(data):
            if time.monotonic() > deadline:
                raise RttError(f'RTT console write stalled after {sent}/{len(data)} bytes')
            try:
                if not select.select([], [sock], [], 0.1)[1]:
                    continue
                sent += sock.send(data[sent:])
            except (BlockingIOError, InterruptedError):
                continue
            except (OSError, ValueError, TypeError, AttributeError) as e:
                # peer death (BrokenPipe/ConnectionReset) or the socket closed under us
                # mid-call: keep the class's one failure contract
                raise RttError(f'RTT console write failed after {sent}/{len(data)} bytes: {e}') from e
        return sent

    def _gentle_stop(self, proc) -> None:
        """Subclass hook: ask the server to exit before the group takedown."""

    def close(self) -> None:
        self._eof = True   # latch: post-close eof reads True, like a hung-up server
        if getattr(self, '_sock', None):
            self._sock.close()
            self._sock = None
        with self._lock:
            self._buf = b''   # pyserial contract: nothing is readable after close
        proc = getattr(self, '_proc', None)
        if proc:
            if proc.poll() is None:
                self._gentle_stop(proc)
                with contextlib.suppress(subprocess.TimeoutExpired):
                    proc.wait(timeout=5)
        if proc and proc.poll() is None:
            # own session (start_new_session), so the group takedown gets the server and
            # anything it spawned; leaving one alive would hold the probe for the next test
            try:
                os.killpg(proc.pid, signal.SIGTERM)
                proc.wait(timeout=5)
            except (ProcessLookupError, PermissionError):
                pass
            except subprocess.TimeoutExpired:
                with contextlib.suppress(ProcessLookupError, PermissionError):
                    os.killpg(proc.pid, signal.SIGKILL)
                # reap, or the server stays a zombie for the caller's lifetime
                with contextlib.suppress(subprocess.TimeoutExpired):
                    proc.wait(timeout=2)
        if proc:
            for pipe in (proc.stdin, proc.stdout):
                if pipe:
                    with contextlib.suppress(OSError, ValueError):
                        pipe.close()
        # the server spool file: one fd plus a /tmp file per console, and the server
        # grows it while alive -- GC is not a release policy on a rig
        log = getattr(self, '_log', None)
        if log:
            with contextlib.suppress(OSError, ValueError):
                log.close()
            self._log = None

    # a console dropped without close() must not hold the probe for the process's life
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def __del__(self):
        with contextlib.suppress(Exception):
            self.close()


class JlinkRtt(_SocketRtt):
    """Bidirectional console over SEGGER RTT channel 0, for J-Link probes (the only
    console on boards whose probe has no VCOM or whose BSP has no UART).

    J-Link Commander (JLinkExe) owns the probe and serves RTT channel 0 on
    -RTTTelnetPort -- what JLinkRTTClient talks to, minus its banner. It keeps
    hunting for the control block and streams whatever the buffer already holds,
    where JLinkRTTLogger searches once when it attaches and gives up. It also
    carries input, which the host tests that drive a menu need.

    The probe is held for as long as this is open, so flashing and resetting the
    board must happen before it is created or after close(). Select the probe by
    serial: rigs run more than one."""

    server = 'JLinkExe'

    def __init__(self, board: dict, timeout: float = 0.1):
        super().__init__(timeout)
        flasher = board['flasher']
        args = shlex.split(flasher.get('args', ''))
        if '-device' not in args:
            # fail with the real cause now: JLinkExe without a device blocks prompting
            # and would surface 15 s later as a misleading port error
            raise RttError(f'RTT console: no -device in flasher args: {flasher.get("args")!r}')
        port = free_ports(1)[0]
        # defaults first, the roster's args after so they can override (-if jtag,
        # -JLinkScriptFile, an explicit -speed). NOTE: hil_flash orders it the other
        # way (roster args first, its own -if/-speed last, so ITS defaults win) --
        # a roster override honored here is ignored by flash/reset; align them if a
        # roster ever carries such args. -ExitOnError makes a failed target connect
        # EXIT Commander
        # (a clean error with the log tail) instead of leaving a banner-only console
        cmd = ['JLinkExe', '-USB', str(flasher['uid']), '-if', 'swd',
               '-JTAGConf', '-1,-1', '-speed', 'auto', '-NoGui', '1',
               '-ExitOnError', '1', '-AutoConnect', '1',
               *args, '-RTTTelnetPort', str(port)]
        # stdin stays open: Commander exits when it runs out of input; close() writes
        # 'exit' there.
        self._spawn(cmd, stdin=subprocess.PIPE)
        self._connect(port)

    def _gentle_stop(self, proc) -> None:
        with contextlib.suppress(OSError, ValueError):
            proc.stdin.write(b'exit\n')
            proc.stdin.flush()
        # close our pipe end in its own suppress: a BrokenPipe on the write above must
        # not skip it (the base close also closes it for the server-already-dead path)
        with contextlib.suppress(OSError, ValueError):
            proc.stdin.close()


class OpenocdRtt(_SocketRtt):
    """The console surface over an openocd `rtt server` (native probes:
    ST-Link/CMSIS-DAP — never point openocd at ea4088's LPC-Link2, measured to
    knock that probe off USB; other J-Link-OB probes untested).

    Exact control-block address (never a full-RAM scan), polling_interval 1
    (default 100 ms polling loses most of a busy stream), attach WITHOUT reset —
    flash and reset before starting; `rtt start` needs the block to exist.
    reset_before_attach opts into an in-session reset for streams that only
    decode from byte 0 (SystemView)."""

    server = 'openocd'

    def __init__(self, cfg: str, addr: int, channel: int, serial_no: str = None,
                 vid_pid: str = None, timeout: float = 0.1, reset_before_attach: bool = False):
        super().__init__(timeout)
        port = free_ports(1)[0]
        # argv, never a shell string: cfg/serial/vid_pid come from roster JSON and the
        # command line, and a '$', backtick or quote in any of them would otherwise be
        # substituted by the shell or break out of it
        cmd = ['openocd', '-c', 'tcl_port disabled', '-c', 'gdb_port disabled',
               '-c', 'telnet_port disabled']
        # probe pin: vid_pid keeps discovery from opening foreign usbfs nodes (a
        # wedged one hangs the open), serial disambiguates same-model probes —
        # both before the -f scripts, like hil_flash does
        if vid_pid:
            if not re.fullmatch(r'0x[0-9a-fA-F]{1,4} 0x[0-9a-fA-F]{1,4}', vid_pid.strip()):
                # openocd only WARNS and exits 0 on a malformed value, so the pin
                # silently does not apply and discovery reopens every usbfs node --
                # the convoy hil_flash.valid_vid_pid exists to stop
                raise RttError(f'--vid-pid must be "0xVVVV 0xPPPP", got {vid_pid!r}')
            cmd += ['-c', f'adapter usb vid_pid {vid_pid.strip()}']
        if serial_no:
            cmd += ['-c', f'adapter serial {serial_no}']
        cmd += shlex.split(cfg)
        cmd += ['-c', 'init']
        # opt-in: reset the target INSIDE this session, give it 2 s to boot, THEN
        # attach and drain. The order is forced: `rtt start` needs the control block
        # to already exist in RAM (the firmware creates it at init), and attaching
        # ahead of the reset would latch the PREVIOUS run's stale block. Byte 0 still
        # reaches the consumer because NO_BLOCK_SKIP retains the ring's HEAD: a boot
        # burst bigger than the ring loses its tail until the drain catches up, never
        # its first bytes -- which is the part a boot-anchored decoder needs
        # (SystemView's Init record, carrying the timestamp frequency, is emitted once
        # at boot; a mid-flight attach yields a stream no decoder can lock onto; size
        # BUFFER_SIZE_UP to the boot burst if the tail matters too). Costs the tool's
        # usual no-reset invariant, and is unsafe on parts where an in-session reset
        # leaves the core held (SAMD5x DSU) or perturbs the target (WCH SDI).
        if reset_before_attach:
            cmd += ['-c', 'reset run', '-c', 'sleep 2000']
        cmd += ['-c', f'rtt setup 0x{addr:x} 0x800 "SEGGER RTT"',
                '-c', 'rtt polling_interval 1', '-c', 'rtt start',
                '-c', f'rtt server start {port} {channel}']
        self._spawn(cmd)
        self._connect(port)

    def _gentle_stop(self, proc) -> None:
        # no stdin channel to ask openocd to exit, and it keeps its listener up after
        # the client disconnects: go straight to the group takedown instead of blocking
        # the base class's 5 s wait on a process that has no reason to leave
        with contextlib.suppress(ProcessLookupError, PermissionError):
            os.killpg(proc.pid, signal.SIGTERM)


def dump_ring(probe: str, device: str, addr: int, out_path: str, channel: int = 0) -> int:
    """Post-mortem: read aUp[channel]'s ring over the debug AP (no halt) via JLinkExe.
    NO_BLOCK_SKIP means an undrained ring holds the FIRST KB after boot, not the
    tail — interpretation rules in the target-debug skill."""
    if re.search(r'[\s"\']', out_path):
        raise SystemExit(f'--dump path must not contain whitespace or quotes: {out_path!r} '
                         f'(it is spliced into a JLinkExe script line)')
    # a stale file from an earlier run must not satisfy the success check below
    with contextlib.suppress(OSError):
        os.remove(out_path)
    # SEGGER_RTT_CB: acID[16], MaxNumUpBuffers, MaxNumDownBuffers, then aUp[] at 0x18,
    # each ring 6 words {sName, pBuffer, SizeOfBuffer, WrOff, RdOff, Flags}. Read the
    # counts with the descriptor so an out-of-range channel is rejected instead of
    # reading whatever RAM follows the array.
    jlink = ['JLinkExe', '-USB', probe, '-device', device, '-if', 'swd',
             '-speed', '4000', '-NoGui', '1', '-AutoConnect', '1']

    def _jlink_run(script: str):
        # same clean-exit contract as nm_rtt_addr/_spawn: a missing binary or a wedged
        # probe must not reach the CLI as a traceback
        try:
            return subprocess.run(jlink, input=script, capture_output=True, text=True, timeout=60)
        except FileNotFoundError:
            raise SystemExit('JLinkExe not on PATH — the --dump route needs J-Link Commander')
        except subprocess.TimeoutExpired:
            raise SystemExit('JLinkExe did not finish in 60 s — probe wedged or target unreachable?')

    script = f'mem32 {addr + 0x10:#x}, 2\nmem32 {addr + 0x18 + channel * 24:#x}, 6\nexit\n'
    r = _jlink_run(script)
    words = []
    for line in r.stdout.splitlines():
        # UNANCHORED: when the script arrives on stdin, some JLinkExe versions glue
        # the 'J-Link>' prompt onto the result line with no newline between
        m = re.search(r'([0-9A-Fa-f]{8}) = ((?:[0-9A-Fa-f]{8} ?)+)$', line.strip())
        if m:
            words += [int(w, 16) for w in m.group(2).split()]
    if len(words) < 8:
        print(r.stdout[-500:], file=sys.stderr)
        raise SystemExit(f'could not read the aUp[{channel}] descriptor — wrong control block address?')
    max_up = words[0]
    if not 0 < max_up <= 32:
        raise SystemExit(f'control block at {addr:#x} looks uninitialized '
                         f'(MaxNumUpBuffers={max_up}) — the target has not written to RTT yet, '
                         f'or the address is wrong')
    if channel >= max_up:
        raise SystemExit(f'--channel {channel}: this firmware has {max_up} up-buffer(s) (0..{max_up - 1})')
    _, pbuf, size, wroff, rdoff, _ = words[2:8]
    if not pbuf or not size:
        raise SystemExit(f'up-buffer {channel} is not initialized (pBuffer={pbuf:#x} size={size}) — '
                         f'the target has not written to it yet')
    script = f'savebin {out_path}, {pbuf:#x}, {size:#x}\nexit\n'
    _jlink_run(script)
    # JLinkExe exits 0 even when a command inside its script fails, so the only proof
    # savebin worked is the file itself: it must hold the WHOLE ring, since a read that
    # dies partway (probe disconnect, unreadable address) still leaves a short file that
    # would otherwise be reported as a complete dump. Removing it also keeps the
    # invariant above -- no stale file can satisfy a later run's check.
    got = os.path.getsize(out_path) if os.path.exists(out_path) else 0
    if got < size:
        with contextlib.suppress(OSError):
            os.remove(out_path)
        if got == 0:
            raise SystemExit(f'savebin produced no data at {out_path} — probe or address problem')
        raise SystemExit(f'savebin wrote {got}/{size} B to {out_path} (truncated dump removed) '
                         f'— probe or address problem')
    print(f'ring: {size} B at {pbuf:#x}, WrOff={wroff:#x} RdOff={rdoff:#x} -> {out_path}\n'
          f'valid bytes wrap at WrOff; default NO_BLOCK_SKIP holds the FIRST data after '
          f'boot, not the tail', file=sys.stderr)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--backend', choices=['jlink', 'openocd'], required=True,
                    help='transport route — explicit, no default (skill transport matrix)')
    ap.add_argument('--probe', help='probe serial (JLinkExe -USB / openocd "adapter serial")')
    ap.add_argument('--vid-pid', help='openocd probe pin by USB IDs, e.g. "0x2e8a 0x000c" '
                                      '(with or instead of --probe)')
    ap.add_argument('--device', help='JLINK_DEVICE from board.cmake/family.cmake (jlink backend)')
    ap.add_argument('--cfg', help='openocd -f/-c args, e.g. "-f interface/stlink.cfg -f target/stm32h7x.cfg"')
    ap.add_argument('--elf', help='the FLASHED elf: exact _SEGGER_RTT address via nm (openocd/--dump)')
    ap.add_argument('--addr', help='SEGGER RTT control block address (hex), instead of --elf')
    ap.add_argument('--channel', type=int, default=0, help='up-buffer index (0 console, 1 SysView)')
    ap.add_argument('--seconds', type=float, default=0, help='capture duration; 0 = until Ctrl-C/EOF')
    ap.add_argument('-i', '--interactive', action='store_true', help='forward stdin to the target')
    ap.add_argument('--reset-before-attach', action='store_true',
                    help='openocd: reset the target inside the capture session so the '
                         'server is draining when it boots (needed for streams that must '
                         'include the boot preamble, e.g. SystemView); unsafe on SAMD5x/WCH')
    ap.add_argument('--dump', metavar='OUT.bin',
                    help='post-mortem ring dump (jlink backend; needs --elf or --addr)')
    args = ap.parse_args()

    if args.seconds < 0 or args.seconds != args.seconds:   # negative or nan
        ap.error(f'--seconds must be >= 0 (0 = until Ctrl-C/EOF), got {args.seconds}')
    if args.channel < 0:
        # a negative index would walk backwards off aUp[] into the control-block
        # header and read garbage as a descriptor
        ap.error(f'--channel must be >= 0, got {args.channel}')

    def rtt_addr():
        if args.addr:
            try:
                return int(args.addr, 16)
            except ValueError:
                ap.error(f'--addr must be hex, got {args.addr!r}')
        if args.elf:
            return nm_rtt_addr(args.elf)
        ap.error('need --elf (flashed elf, address via nm) or --addr')

    if args.backend == 'jlink':
        if args.reset_before_attach:
            ap.error('--reset-before-attach is openocd-only (the J-Link route attaches '
                     'to a running target; flash and reset before starting it)')
        if args.channel and not args.dump:
            # -RTTTelnetPort serves the Terminal buffer only; --dump can read any ring
            ap.error('the jlink backend streams channel 0 only (use --backend openocd '
                     'for another channel, or --dump to read one)')
        if args.vid_pid:
            ap.error('--vid-pid is openocd-only; J-Link probes are selected by serial (--probe)')
        if not (args.probe and args.device):
            ap.error('the jlink backend needs --probe and --device')
    elif not (args.probe or args.vid_pid):
        ap.error('the openocd backend needs --probe and/or --vid-pid')

    if args.dump:
        if args.backend != 'jlink':
            ap.error('--dump uses the jlink backend (debug-AP reads via JLinkExe)')
        return dump_ring(args.probe, args.device, rtt_addr(), args.dump, args.channel)

    # install BEFORE the console exists: an external `timeout`/kill during the
    # up-to-15 s connect window must still reach the cleanup below, or the openocd
    # route leaves a server holding the probe and the port (JLinkExe would exit on
    # stdin EOF; openocd has no such channel and its own session shields it)
    def _terminate(signum, _frame):
        raise KeyboardInterrupt
    for _sig in (signal.SIGTERM, signal.SIGHUP):
        with contextlib.suppress(ValueError, OSError):
            signal.signal(_sig, _terminate)

    try:
        if args.backend == 'openocd':
            if not args.cfg:
                ap.error('--backend openocd needs --cfg')
            con = OpenocdRtt(args.cfg, rtt_addr(), args.channel,
                             serial_no=args.probe, vid_pid=args.vid_pid,
                             reset_before_attach=args.reset_before_attach)
        else:
            con = JlinkRtt({'flasher': {'uid': args.probe, 'args': f'-device {args.device}'}},
                           timeout=0.1)
    except RttError as e:
        print(e, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130   # constructors clean up after themselves on the way out

    saw_output = threading.Event()
    forwarded = threading.Event()
    if args.interactive:
        def pump_stdin():
            # Hold input until the capture side has seen TARGET output (or 5 s for a
            # quiet firmware): the J-Link telnet route silently DROPS client bytes
            # until Commander locates the control block, so input forwarded at attach
            # vanishes (measured on the rig: instant 'ping' lost, delayed 'ping'
            # echoed). The gate must ignore the server's own banner — it arrives at
            # connect, BEFORE the block is found. Raw os.read, not sys.stdin.buffer:
            # bytes with no newline wait, and no BufferedReader lock — a daemon
            # thread blocked holding that lock at interpreter shutdown aborts
            # CPython (_enter_buffered_busy).
            saw_output.wait(5)
            try:
                while True:
                    data = os.read(0, 4096)
                    if not data:
                        return
                    con.write(data)
                    forwarded.set()
            except (RttError, OSError, ValueError):
                return   # console closed/stalled/dead; capture side reports the state
        threading.Thread(target=pump_stdin, daemon=True).start()

    deadline = time.monotonic() + args.seconds if args.seconds else None
    rc = 0
    seen = b''   # pre-release accumulator for the banner check only
    try:
        while deadline is None or time.monotonic() < deadline:
            try:
                chunk = con.read(con.in_waiting or 1)
            except RttError as e:
                print(f'rtt: {e}', file=sys.stderr)
                rc = 1
                break
            if chunk:
                if args.interactive and not saw_output.is_set():
                    # target data = anything past the J-Link banner's final line
                    # ('Process: <name>'); the openocd server has no banner
                    seen = (seen + chunk)[-65536:]
                    if args.backend != 'jlink':
                        saw_output.set()
                    else:
                        i = seen.find(b'Process: ')
                        j = seen.find(b'\n', i) if i >= 0 else -1
                        if j >= 0 and len(seen) > j + 1:
                            saw_output.set()
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
            elif con.eof:
                print('rtt: server closed the connection', file=sys.stderr)
                rc = 1
                break
    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        # downstream consumer (head/grep -m) closed the pipe: a normal way to end a
        # capture, not an error. Point stdout at devnull so interpreter shutdown does
        # not raise on the final implicit flush.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
    finally:
        if args.interactive and not forwarded.is_set():
            # only claim what is true: the gate releases after 5 s and forwards anyway,
            # so "never forwarded" must come from the forwarded flag, not the gate
            print('rtt: -i stdin was never forwarded to the target (no input arrived, '
                  'or the console closed first)', file=sys.stderr)
        if args.interactive and not saw_output.is_set():
            print('rtt: no target output within the window', file=sys.stderr)
        # a late TERM landing during the up-to-12 s teardown must not skip the kill
        # escalation and orphan the server -- cleanup is committed at this point
        for _sig in (signal.SIGTERM, signal.SIGHUP):
            with contextlib.suppress(ValueError, OSError):
                signal.signal(_sig, signal.SIG_IGN)
        con.close()
    return rc


if __name__ == '__main__':
    sys.exit(main())
