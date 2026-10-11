# USBH abort and endpoint lifetime follow-up plan

**Origin:** [PR #3991](https://github.com/hathach/tinyusb/pull/3991).

**Goal:** Define and enforce endpoint ownership when cancellation fails or a
completion is pending during endpoint close and reuse.

**Architecture:** Address this in common USBH and its HCD contract, with native
fake-HCD regression tests. Keep the ChipIdea ISO scheduler independent of this
work. Choose the reuse policy before implementing cross-OSAL synchronization.

**Tech stack:** C99, TinyUSB USBH/HCD, OSAL queues, CMake/CTest.

## Why deferred

The maintainer requested that failed-abort handling also be deferred. PR #3991
therefore retains the previous USBH abort/close behavior. The attempted ownership
fix and its fixture are removed; the macOS EHCI fixture skip remains.

These are existing common-stack limitations. Changing ownership after abort
affects HCDs with unimplemented cancellation and HCDs with asynchronous retirement,
and requires a separate contract review and validation campaign.

## Evidence

- `src/host/usbh.c`: non-control `tuh_edpt_abort_xfer()` ignores the HCD return,
  clears BUSY/CLAIMED, and returns true. EP0 also ignores the HCD return.
- OHCI, RP2040, MUSB, and RUSB2 return false from their unimplemented abort paths.
  EHCI bulk/interrupt returns false if the descriptor has already completed.
- DWC2 periodic DMA accepts cancellation but rejects replacement submissions
  until the halt ISR retires the channel and suppresses its completion.
- `tuh_task_ext()` dispatches a completion through the endpoint's current
  callback after clearing its current ownership. An event already queued in
  USBH is outside the HCD's cancellation mechanism.
- A native reproduction at `6c3bf1259` queued an old completion of 16 bytes,
  closed/reopened the endpoint, submitted a new transfer with different callback
  data, then ran the USBH task. The new callback received the old 16-byte result,
  and the new transfer's BUSY flag was cleared. Source inspection shows that the
  previous abort/close path also allowed this reuse; the lifetime problem was
  not introduced by the ownership fix.
- Reviews: [failed abort](https://github.com/hathach/tinyusb/pull/3991#discussion_r4112166280),
  [queued completion](https://github.com/hathach/tinyusb/pull/3991#discussion_r4112231165),
  [close/reopen regression](https://github.com/hathach/tinyusb/pull/3991#discussion_r4112236957).

## Remaining work

### 1. Recover and extend the regression fixture in a separate branch

Files: `test/unit-test/host/usbh/test_abort.c`, its `tusb_config.h`, and
`test/unit-test/host/CMakeLists.txt`.

- [ ] Recover the fake-HCD fixture and target registration from commit
  `6c3bf1259`; the attempted production fix is in `9d77cb78a`.
- [ ] Confirm its failed-abort assertion fails against the restored USBH code.
- [ ] Add IN and OUT cases for this sequence: submit A, queue A's completion,
  close, reopen, attempt B, process the old event. Assert that A never invokes
  B's callback or clears ownership of an accepted B. If reuse is deferred,
  assert that B can be submitted after old-event retirement.
- [ ] Cover failed close, successful cancellation, delayed cancellation,
  unrelated queued events, and class callbacks as well as application callbacks.

Run the native fixtures with:

```sh
cmake -S test/unit-test/host -B build/host-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-test
ctest --test-dir build/host-test --output-on-failure
```

### 2. Settle the common contract before changing implementation

Files: `src/host/hcd.h`, `src/host/usbh.h`, `src/host/usbh.c`, and affected HCDs.

- [ ] Distinguish unsuccessful cancellation, accepted deferred cancellation,
  completed-but-undispatched transfers, and successful endpoint closure.
- [ ] Specify when buffers and endpoint storage can be reused, what callbacks
  can still occur, and whether immediate close/reopen submission is supported.
- [ ] Review the unchanged EP0 path separately rather than assuming the
  non-control ownership rule applies to control transfers.
- [ ] Evaluate retirement using existing endpoint-state bits before adding
  memory or public APIs. The maintainer previously rejected an ISO-specific
  generation counter; no replacement design is approved by this handoff.

### 3. Implement and validate the agreed policy

- [ ] Make the native regression cases pass without dropping unrelated events.
- [ ] If queue filtering is chosen, implement synchronization for supported
  OSAL backends: there is currently no selective-removal API, and OSAL-none
  queue receive/send can re-enable interrupts. Do not call `tuh_task()`
  recursively from endpoint close.
- [ ] Check memory and code-size impact, run pre-commit, and build the complete
  example sets for representative EHCI and other affected HCD boards.
- [ ] Exercise abort/close/reopen while transfers are active on hardware,
  including an HCD that cannot cancel and one with deferred retirement.
- [ ] Open a separate USBH PR referencing #3991 and the reproduced sequence.
- [ ] Delete this handoff when that PR lands.
