# Threat model

## What this project does and where untrusted input enters
TinyUSB is a USB host and device stack for microcontrollers, linked into firmware. Most targets have no MMU, so
memory corruption is not contained by a process boundary.

The attacker is the USB peer, with physical access to the port and nothing more:
- device mode: a malicious host — control requests, class requests and payloads (CDC, MSC SCSI, NCM/ECM/RNDIS, ...);
- host mode: a malicious device or hub — received descriptors, class responses, hub status;
- either mode: lengths, counts and indices embedded in that data, and attacker-chosen transfer, reset and disconnect
  sequences.

Frames received over the USB network classes are untrusted. Claim reach from a wider network only when a real
application configuration provides that path.

Trusted: the application, its build configuration, and descriptors and buffers the firmware itself supplies (device
mode). Application callbacks are trusted code, but values they receive still come from the peer: trace their
provenance. A controller event is an attacker capability only when a real driver produces it from peer input.

## Components that matter most / least
- In scope: all of `src/`, including the host hub driver and the FIFO, descriptor and buffer helpers in `common/`;
  and `lib/networking/` (TinyUSB's own DHCP, DNS and RNDIS handling of host-sent data).
- `examples/`: in scope, labelled example-only, with a path reachable in the shipped example.
- `src/portable/`: findings that depend on register, DMA or timing semantics are hardware-unverified; say so. A
  peer-controlled value (such as a received byte count) reaching a buffer without a bound is in scope.
- Out of scope: third-party code fetched by `tools/get_deps.py` (`hw/mcu/*`, `lib/FreeRTOS-Kernel`, `lib/lwip`,
  `lib/fatfs`, `lib/threadx`, `lib/CMSIS_*`, ...), `hw/bsp/`, `tools/`, `docs/`, `test/`. TinyUSB's integration with
  those libraries stays in scope.

## How to exercise it
- libFuzzer + ASan harnesses: `test/fuzz/device/<cdc|msc|net|net_ncm>/_build/<name>`, seeds unpacked to
  `test/fuzz/device/<name>/corpus/` (empty for `net`). Replay with `<binary> -runs=0 <corpus or file>`; rebuild offline
  with `make -C test/fuzz/device/<name> all` (CMake, `test/fuzz/CMakeLists.txt`).
- Harness reach is narrow; absence of a harness crash is not evidence of safety. `cdc`, `msc` and `net` drive the device
  stack through a synthetic controller that delivers bus events and SETUP packets but never completes a transfer, so
  control data stages and class bulk data (MSC SCSI, RNDIS/ECM payloads) are not reached. The MSC and ECM/RNDIS
  drivers are never opened either: `msc` uses endpoint 5 while its build allows five endpoints (0-4), and `net`'s
  descriptor advertises only CDC. `net_ncm` tests the NCM receive validator alone. None covers host mode.
- Unit tests: `cd test/unit-test && ceedling test:all` (device and host classes, hub, FIFO).

## How you rate severity
- Critical: a demonstrated code-execution path — a usable write primitive in a named supported configuration, with
  the overwritten target shown. A sanitizer report alone is not enough; a full exploit is not required.
- High: peer-reachable memory corruption (OOB write, use-after-free, double free), or an out-of-bounds read whose
  data is returned to the peer.
- Medium: a demonstrated denial of service — hard fault, hang, or a stack wedged until reset or re-plug.
- Low: minor, self-recovering disruption. An out-of-bounds read whose data is not returned is reported as "impact
  unverified" unless an effect is demonstrated.
- Default versus non-default configuration changes exposure, not severity. Loss of service caused only by the
  attacker disconnecting their own link is not a finding.

## How reports and patches should look
- State the revision, configuration, role and class, a minimized input with its offline replay command, root cause,
  the path from peer input to the faulty code, impact and recovery behaviour. Say whether it reproduced in a host
  build or is source-only inference.
- Add a regression (fuzzer seed or Ceedling test) that fails before the patch and passes after it.
- Keep patches small: C99, no dynamic allocation, existing APIs and `CFG_*` guards preserved, valid traffic unchanged.

## Anything to leave alone
- Crashes that exist only in a fuzz harness, misuse of a documented API by the application, impossible controller
  events, and unsupported configuration combinations, unless a production path is shown.
- On failure `TU_VERIFY` returns; the default `TU_ASSERT` (`src/common/tusb_verify.h`) calls `TU_MESS_FAILED()` (a log
  only under `CFG_TUSB_DEBUG`) and then `TU_BREAKPOINT()` before returning, regardless of `CFG_TUSB_DEBUG`. Inspect
  the affected configuration's breakpoint expansion and any `TU_ASSERT` override before assuming it returns or rating
  its impact.
- Intended MSC writes or DFU operations, and the absence of application-level authentication: that is application
  policy.
- Physical, electrical, glitching and side-channel attacks.
