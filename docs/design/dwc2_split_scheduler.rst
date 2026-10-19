:orphan:

DWC2 split scheduler requirements and simplification study
==========================================================

Baseline: ``267aca1c5943dc9e659ee631e7880fbf6a15e085`` on
``dwc2-split-sched``, 2026-10-19. This is a requirements inventory and a
proposed design boundary, not a replacement implementation or a claim that
the baseline satisfies every requirement.

A simpler implementation looks feasible if scheduling policy, USB transaction
state, and DWC2 request admission are separated. USB 2.0 alone is insufficient:
the DWC2 programming model and measured queue behavior are also inputs. Fewer
flags or lines do not establish correctness, lower ISR cost, or lower jitter.

The labels below distinguish **USB** requirements, **DWC2** programming rules,
**integration** requirements, **observations** from this review, and proposed
**policy**. An observation is not promoted to a universal hardware guarantee.
Open questions remain explicit. The inventory covers periodic splits and the
nonperiodic/host-stack interfaces with which they share resources.

Terms and ownership
-------------------

* **SS / CS:** start-split / complete-split. One downstream FS/LS transaction
  can require several high-speed transactions.
* **Y0..Y7:** microframes of a full-speed frame. ``(Y-1)7`` precedes Y0;
  ``(Y+1)0`` is in the next frame. Absolute microframe time must distinguish
  these even when the programmed parity is identical.
* **TT:** transaction translator. A single-TT hub shares one TT across its
  downstream ports; a multi-TT hub has independent port TTs. The actual
  selected hub configuration/alternate setting matters.
* **Budgeted:** assigned downstream bus time. **Ready:** software has a legal
  action. **Admitted:** hardware has accepted a request into its queue.
  **Issued:** the transaction actually appeared on USB. These are different
  events; neither endpoint iteration order nor every CHENA write proves wire
  order.
* Endpoint configuration owns interval, route, maximum packet size, and
  reservations. A transaction owns phase, bytes, PID, deadlines, and SS order.
  A channel owns hardware execution and must not outlive its transaction.
* USB ordering is per TT. DWC2 periodic-queue ordering is per controller,
  including native-HS periodic traffic and traffic for different TTs.

USB requirements
----------------

Topology, configuration, and bandwidth
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**U01 -- Route and TT identity (USB/integration).** Resolve a FS/LS device
through any intervening FS hubs to the upstream HS hub and its downstream
port. Keep the endpoint's wire route separate from its scheduling TT key:
single-TT grouping does not mean programming port zero. Recompute or discard
the route on topology changes. Direct FS/LS operation without an HS upstream
segment and native-HS endpoints do not use splits. [U-ROUTE]_ [D-REG]_

**U02 -- Endpoint contract (USB/integration).** Validate transfer type, speed,
maximum packet size, interval, and descriptor encoding before reserving resources.
FS ISO intervals are exponential frame intervals; FS/LS interrupt descriptors
use frame units, unlike HS periodic exponential microframe intervals. USB allows
the host to choose a shorter supported polling period; do not equate the
descriptor's desired period with a mandatory exact interval. As an integration
invariant, preserve the chosen reservation phase across NAKs, resubmission,
delayed completion, and clock wrap. Poll only when a transfer is pending. Do not
apply native-HS high-bandwidth multiplier semantics to FS split endpoints.
[U-ENDPOINT]_

**U03 -- Two bandwidth calculations (USB).** Admission must account for both
the high-speed bus, shared by all TTs/native-HS endpoints, and each TT's
downstream bus. Periodic split traffic consumes the HS periodic allocation;
the limits are 80% of a HS microframe and 90% of a FS/LS frame. Worst-case bus
time includes protocol overhead, bit stuffing, turnaround, low-speed setup,
host delay, and TT think time. Separately, the TT's *best-case* microframe
budget determines when SS/CS actions must be scheduled. Do not substitute a
payload-only ``ceil(bytes/188)`` count for either complete calculation.
[U-BANDWIDTH]_ [U-BUDGET]_

For the best-case calculation, USB 11.18.1 uses 188 raw FS wire bytes per
microframe (physical capacity is 187.5). Its conservative frame budget is
1157 bytes, from ``1500 * 6/7 * 0.90``, including transaction overhead rather
than 1157 payload bytes. TT think time comes from the hub descriptor. Keep
these units separate from HS bus-time costs and from each SS/CS payload length.
[U-BUDGET]_

**U04 -- Reservations and rejection (USB/policy).** Reserve from advertised
maximum payload, not the last short packet or whether an endpoint is currently
idle. A new endpoint/alternate setting must fit the existing schedule or be
rejected explicitly. Reservation release and failed-open rollback must be
complete. Conservative admission is allowed; silent overcommit is not a
complete bandwidth guarantee. A fixed repeating pattern can be computed when
the endpoint set changes. [U-BUDGET]_ [U-SCHEDULE]_

Periodic schedule
~~~~~~~~~~~~~~~~~

**U05 -- Scheduled microframe is a deadline (USB).** A split belongs to its
scheduled microframe; do not replay an expired action simply because its
parity matches again. Never schedule a periodic SS in Y6. At most 16 periodic
SS actions may be issued for one TT in a microframe. These are TT constraints,
not a controller-wide four-slot rule. [U-SCHEDULE]_

**U06 -- SS placement (USB).** Interrupt IN/OUT and ISO IN use one SS in the
microframe before their downstream transaction is budgeted to start. ISO OUT
uses data-bearing SS pieces in the microframes preceding their corresponding
downstream data budget. A first piece can therefore be in ``(Y-1)7``.
[U-SCHEDULE]_

**U07 -- Interrupt CS window (USB).** If the downstream transaction starts its
budget in Ys, schedule CS in the following two microframes and also the third
unless s=6. For an ordinary early SS at absolute time T this gives T+2, T+3,
T+4; it is not an unconditional rule for all frame placements. For the Y6
budget case, Figure 11-63 has SS at Y5 and CS at Y7 and ``(Y+1)0``. Stop unused
CS opportunities when a terminal response arrives. [U-SCHEDULE]_ [U-RESP]_

**U08 -- ISO IN CS window (USB).** Schedule CS after every microframe occupied
by the downstream best-case transaction. Let L be the last such CS slot:

* L before Y6: add L+1 and L+2.
* L=Y6: add Y7, and ``(Y+1)0`` unless the transaction was budgeted to start in Y0.
* L=Y7: add ``(Y+1)0`` unless the transaction was budgeted to start in Y0.

Represent this as an explicit set/window with frame identity. A simplified
supported subset may reject endpoints requiring other placements; it must not
label its subset as the entire USB rule. A 1023-byte ISO IN can require up to
eight CS opportunities because budget alignment and bit stuffing affect the
returned pieces. [U-SCHEDULE]_ [U-RESP]_

**U09 -- Scheduled attempt versus immediate retry (USB/DWC2).** Normally
schedule no more than one CS per downstream transaction per microframe.
Immediate retries after a high-speed transaction error are a separate protocol
operation required by the state machines. Do not confuse NYET's next scheduled
CS opportunity with an immediate error retry, or accidentally issue duplicate
ordinary CS actions from SOF and channel IRQ paths. [U-SCHEDULE]_ [D-REG]_

**U10 -- Per-TT order (USB).** For periodic transactions sharing a TT, CS actions
in a microframe must preserve the corresponding SS relative order. Track actual
admission/issuance, including deferred OUT data, rather than channel number,
endpoint allocation order, or planned time alone. A younger CS can make a TT
advance past and discard an older result. An older transaction blocks only
while it can still require an earlier/equal relevant CS; completion, cancellation,
and error transitions must remove that dependency. [U-PIPELINE]_

**U11 -- TT response lifetime (USB).** Periodic TT results are not a persistent
mailbox. Pipeline state advances at microframe boundaries, and old CS entries
are freed. Preserve consecutive scheduled opportunities while continuation is
needed. After a missed ISO continuation slot, do not concatenate a later tail
with earlier bytes and claim a complete packet. TT pipeline advancement also
continues when the host fails to service it. [U-PIPELINE]_ [U-ISO]_

**U12 -- Window exhaustion (USB).** Terminal NYET in the last scheduled CS is
an error condition, not an unlimited extension of the window. Apply the
specified endpoint error policy; the current driver's decision not to count
this NYET is an explicit policy deviation to review. ISO without a terminal
DATA0 by its last CS is an error and advances to the next transaction. An
interrupt retry in a later service interval is a new transaction with preserved
endpoint synchronization, not a continuation of expired TT state.
[U-PIPELINE]_ [U-ISO]_

Transaction data and responses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**U13 -- ISO OUT pieces (USB).** Send at most 188 data bytes per SS, with
ALL for a one-piece packet or BEGIN/MID/END for a multi-piece packet. ALL=3,
BEGIN=2, MID=0, END=1 in HCSPLT.XactPos. A zero-length payload uses ALL only;
BEGIN/MID/END must be nonempty. Never schedule CS for ISO OUT. Keep the pieces
in their assigned microframes, and fail rather than replay a late piece.
[U-SCHEDULE]_ [U-ISO]_ [D-REG]_

**U14 -- Periodic SS handshake distinction (USB/DWC2).** Periodic SS have no
TT handshake on the HS wire, including interrupt IN/OUT and ISO IN/OUT.
DWC2's ACK interrupt for such an SS is controller progress information;
it does not prove a downstream device ACK. ISO OUT has no downstream retry
guarantee and no CS phase. Do not apply an interrupt-OUT SS/CS state machine
to it. [U-RESP]_ [U-ISO]_ [D-SLAVE]_

**U15 -- IN fragments and terminal PID (USB).** MDATA means continuation for
both interrupt IN and ISO IN. Accumulate it and request the next scheduled CS.
DATA0 terminates ISO IN; DATA0/DATA1 terminates interrupt IN and carries its
data-toggle meaning. Actual fragment lengths may be irregular and less than
188 bytes; do not infer completion from a short fragment alone. The final
DATA0 can legitimately be short. Capacity exhaustion, overflow, and missing
terminal PID require explicit outcomes; filling the caller's buffer is not
proof that the downstream transaction completed. [U-RESP]_ [U-ISO]_

Interrupt-IN fragments are tentative until the whole downstream transaction
is valid: USB 11.20.4 requires a CRC failure or ERR on a partial CS to invalidate
previously accepted bytes from that transaction. A later fragment can therefore
require rollback of an earlier MDATA fragment. Do not deliver that prefix as
successful interrupt data or advance the toggle before terminal validation;
keep immediate HS retry handling distinct from committing the assembled packet.
Rejecting the terminal data toggle must also discard the tentative transaction,
without publishing its earlier fragments as a new report.
[U-RESP]_

**U16 -- Response table (USB).** Interpret a response using transfer type,
direction, SS/CS phase, and error history. At minimum distinguish:

* SS progress/ACK from delivery of the downstream transaction.
* CS NYET (not ready; next legal opportunity) from CS NAK (device has no data
  or did not accept data; end this periodic attempt).
* MDATA continuation from terminal DATA0/DATA1, and OUT completion ACK.
* STALL, TT ERR, HS CRC/timeout/transaction error, babble, toggle mismatch,
  and missed deadline.
* An immediate CS retry from a new SS attempt. Avoid duplicate delivery or
  advancing an endpoint's toggle twice.

The scheduler consumes these decoded outcomes; it should not reinterpret raw
HCINT combinations independently in several paths. USB 11.17, 11.20, 11.21
and DWC2 mode-specific examples define the cases. [U-RESP]_ [D-SLAVE]_ [D-DMA]_

**U17 -- Nonperiodic splits remain distinct (USB).** Control/bulk SS acceptance,
CS NYET retries, duplicate handling, and error recovery use persistent TT buffers
and different state machines from periodic traffic. They must continue to make
progress without stealing reserved periodic bus time. USB 11.17.5 requires the
hub's CLEAR_TT_BUFFER request as part of endpoint-halt processing for FS/LS
control/bulk endpoints behind a TT, to release a possibly busy TT buffer.
Coordinate that with the hub layer rather than merely freeing a local channel.
Native-HS PING is not split-CS retry handling. [U-NONPERIODIC]_

DWC2 execution requirements
----------------------------

**D01 -- Capabilities (DWC2/integration).** Validate the instantiated core's
host/split support, channel count, FIFO RAM, request-queue depths, core revision,
PHY clock, and enabled transfer mode. Buffer DMA and slave mode have split
programming models; the cited DWC2 core does not support host splits in
scatter/gather DMA mode. Do not infer capabilities from the MCU family name
or a generic maximum of 16 channels. [D-ARCH]_

**D02 -- Shared queue (DWC2).** Periodic channels share one request queue and
periodic TX FIFO; nonperiodic channels have another queue/TX FIFO. All IN
traffic shares RX resources. An independent TT does not imply independent
controller resources. Queue entries include non-data operations such as channel
halt. FIFO free words and request-queue free entries are separate resources.
[D-ARCH]_ [D-FIFO]_ [P4]_

**D03 -- Admission point (DWC2).** In slave mode, IN is queued by channel
enable; a nonempty OUT is queued with the final DWORD of the packet FIFO write.
A zero-length OUT has a separate enable-based programming sequence. Check
queue and FIFO capacity before writing, pad only the FIFO access as required,
and preserve the programmed wire byte count. Reserve scheduling state before
enable when needed, but assign SS order only at the correct admission point.
This distinction is the original H2 fix. [D-FIFO]_ [EVIDENCE]_

**D04 -- DMA is not slave admission (DWC2/open).** Buffer-DMA IN enters the
request queue when its channel receives an arbitration grant; nonempty OUT
enters at the end of its data fetch into the FIFO. CPU CHENA order alone is
not a specification proof of DMA wire order. Establish a supported
admission/ordering contract through the arbitration
description and hardware tests. If it cannot be proven, constrain admission or
serialize the relevant same-TT starts without missing their windows. The
existing slave FIFO test does not establish this DMA property. [D-ARCH]_ [D-DMA]_

**D05 -- Parity is not an absolute timestamp (DWC2).** HCCHAR.OddFrm only
selects odd/even periodic frames/microframes. Software owns the full deadline,
the one-microframe look-ahead policy, and expiration. Check capacity before the
time sample that selects parity; revalidate after any wait that can cross a
boundary. Protect the time-to-enable decision against relevant ISR/task races.
A same-parity request serviced two microframes later is late. [D-REG]_

**D06 -- Current work before future queue blockers (observation/integration).**
The H1 experiment demonstrates a current CS delayed behind future periodic
work, across TTs. Do not admit a future-parity periodic request ahead of a
current request that pending RX/channel work can still produce. Apply the
constraint to the shared queue, including ISO OUT pieces and native-HS periodic
retries, not only same-TT CS. Conversely, unrelated control RX/HC work alone
must not consume a periodic request's prequeue opportunity: the broad guard
caused a captured startup regression. No universal current-microframe
preemption guarantee was established. [EVIDENCE]_

**D07 -- RX can create channel interrupts (DWC2).** In slave mode, popping
receive-status entries can make XferCompl or ChHltd visible. Drain each
reported data payload correctly and reread live channel-interrupt state after
RX processing. A saved entry-time HCINT bitmap is insufficient. If deferral
was caused by RX work that produces no HCINT, service ready requests again
within the same IRQ instead of waiting for another SOF. [D-HALT]_ [D-SLAVE]_

**D08 -- Preserve interrupt latency (user/integration).** Preserve prompt SOF
handling and RX-before-live-HC processing. A rewrite must demonstrate that it
does not add a second IRQ for a completion already exposed by RX processing.
Separate clock/state updates from hardware admission without moving all
time-critical work behind unbounded FIFO draining. This is an explicit project
constraint, not a claim that USB mandates one particular C handler order.

**D09 -- Register protocol (DWC2).** Initialize HCSPLT route/phase/position,
HCCHAR endpoint/parity, HCTSIZ bytes/packet count/PID, HCDMA when applicable,
and interrupt masks in the mode-specific sequence before enable. Respect
write-one-to-clear/read-to-pop semantics; do not clear a newly generated event
using a stale snapshot. Decode combined halt/cause bits by mode and phase.
The split EC field controls immediate error retries and must be nonzero;
do not confuse its meaning with nonsplit MC. Distinguish TT ERR from exhausted
HS retries as described by the programming guide. [D-REG]_ [D-HALT]_

**D10 -- Halt and reuse (DWC2).** A posted/in-flight channel cannot be reused
just because software no longer wants its result. Follow the halt protocol,
including slave queue capacity and RX status consumption. In DMA mode, the
guide forbids arbitrary disable of split-enabled periodic *and nonperiodic*
channels; only its specified scenarios, such as NAK/FrmOvrn handling, permit
the write. It separately prohibits disabling nonsplit periodic channels.
Do not copy the slave cancel sequence blindly. Once an idle waiting channel is
provably unposted, software can retire it without inventing a pending hardware halt.
Disconnect/reset recovery must invalidate queued work before reuse. [D-HALT]_

**D11 -- DMA memory and byte accounting (integration).** Provide DMA-accessible,
correctly aligned buffers and cache maintenance for the actual integration.
Irregular IN fragment lengths can make a cumulative user-buffer offset
unaligned: use an aligned staging buffer or another proven transfer strategy.
Track requested capacity, hardware transfer size, FIFO bytes, valid bytes, and
delivered bytes separately. Retrying an error must not copy the same fragment
twice or expose stale padding. The current 192-byte ISO-IN bounce buffer is an
implementation choice, not a USB packet-size requirement.

**D12 -- Bounded resource waits (integration/policy).** Channel shortage,
queue-full, TX FIFO-full, and RX pressure must have explicit pending/retry
outcomes. An ISR spin that waits for a future-parity queue head can consume
the deadline it is trying to meet. Prefer nonblocking submission with a
well-defined wakeup; bound all scans/drain work and reserve queue capacity for
required halts. Derive any FRREM safety margin from the actual PHY clock and
measured worst-case service time, not a universal magic cycle count.
[D-FIFO]_ [D-HALT]_

Register and adapter checklist
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The implementation should have one documented contract for each group below.
This is a checklist for the adapter, not a new register-access abstraction or
a claim that all silicon variants have been audited.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Register/interface
     - Contract to establish
   * - HFNUM / HFIR / PHY configuration
     - Frame versus microframe numbering, counter width/wrap, FRREM units,
       SOF acknowledgement, and the time/parity sampling boundary.
   * - HCCHAR / HCSPLT
     - Device/endpoint/speed/type versus hub/port route; SS versus CS;
       ISO OUT position; periodic parity; EC immediate-retry count;
       enable/disable ownership. Set all phase-dependent fields explicitly.
   * - HCTSIZ / HCDMA
     - Separate OUT wire length from IN receive capacity, packet count, PID,
       residual length, and DMA ownership. Follow the split-specific programming
       sequence; do not blindly copy a nonsplit multi-packet setup. Never set
       DoPng for an IN channel. [D-XFER]_
   * - GINTSTS/GINTMSK, HAINT/HAINTMSK, HCINT/HCINTMSK
     - Decode the interrupt hierarchy and combined causes from live state;
       distinguish event bits from level indications. Channel event bits are
       W1C. Acknowledge the consumed event before reusing its state, and choose
       masks by phase and mode. HALTED alone does not say success. [D-EVENT]_
   * - GRXSTSR / GRXSTSP / receive FIFO
     - Status peek versus pop; channel, packet status, PID and byte count;
       payload DWORD draining; RX status that exposes another interrupt.
       Pop only while RxFLvl is asserted. Diagnostic reads must not consume
       the driver's status. [D-EVENT]_
   * - HPTXSTS / HNPTXSTS / transmit FIFO
     - Free request entries versus free words; channel selected for FIFO data;
       last-DWORD admission; DWORD padding; FIFO-empty interrupt wakeups;
       capacity reserved for halts. Queue-top diagnostics alone do not prove
       a request is actively blocking another one. [D-FIFO]_ [D-HALT]_
   * - Core configuration / reset / port / memory interface
     - Detect supported mode and FIFO sizes, initialize the core, serialize
       flush/reset against active channels, handle disconnect, and apply the
       target's cache, barriers, interrupt-exclusion and errata requirements.

The databook's general IN size rule reserves whole maximum-packet multiples;
split examples and the integration's memory requirements must be reconciled
with that rule before choosing a staging-buffer size. The current 192-byte
buffer is not itself a specification proof of safe DMA bounds. [D-XFER]_

Runtime and lifecycle requirements
----------------------------------

**R01 -- One time model.** Use absolute/extended microframe deadlines or a
documented modular-time abstraction. The baseline masks comparisons to 14 bits;
signed modular comparisons are valid only inside their half-range. Long
intervals need wider state/countdowns. Handle multiple elapsed microframes when
SOF service is delayed rather than decrementing once per observed IRQ.

**R02 -- One owner for submission.** SOF, RX, channel IRQ, FIFO-empty, and task
submission paths may make a transaction ready; they must share the same legal
admission decision. Prevent duplicate SS/CS, stale parity, simultaneous reuse,
and recursive scheduling through transfer-complete callbacks.

**R03 -- Stable transaction identity.** Store TT/endpoint identity, service
generation, phase, SS time/order, CS opportunities, byte offset, and terminal
status explicitly. Use a wrap-safe order representation whose range is proven
for channel reuse and all active TTs. Do not infer transaction identity solely
from mutable channel registers or a reused channel index.

**R04 -- Cancellation and hotplug.** Closing, aborting, device removal, hub
removal, reset, suspend/resume, and alternate-setting changes must invalidate
future actions and release TT/ISO/bounce reservations exactly once. Ignore late
events belonging to an old transaction generation. Do not issue further SS/CS
after cancellation or report a transfer twice.

**R05 -- ISO OUT ownership.** Model one contiguous ISO OUT packet's reserved
SS sequence on its TT. The current implementation holds interrupt SS between
BEGIN and END and releases them after END is admitted. Preserve that observed
compatibility behavior unless an alternative is justified and tested. CS may
occur between pieces. The blanket exclusion rule is retained as a hardware
observation/policy, not asserted here as a directly quoted USB MUST.
[EVIDENCE]_

**R06 -- Failure is observable.** Distinguish successful terminal completion,
short packet, partial data with error, missed service, cancelled transfer,
and resource/admission failure. Specify what the HCD API returns for each.
Application sample continuity and bus scheduling success are different metrics;
a nominally good capture does not prove sample-loss-free service.

**R07 -- Coexistence.** Enumeration/control retry storms, bulk disk traffic,
native-HS periodic traffic, FS/LS interrupt, ISO IN/OUT, and another TT must
coexist. FS direct-root operation, native HS, hub-disabled builds, slave-only,
DMA-only, and dual-mode builds must retain their existing contracts. No heap
allocation or class callbacks should be added to the deadline-critical path.

**R08 -- Integration audit.** Before claiming portability, check each target's
core revision/configuration, PHY/FRREM units, FIFO layout, memory/cache rules,
interrupt routing and priority, SMP exclusion, and MCU errata. The P4 TRM
documents 16 channels, 4 KB shared FIFO RAM, and four entries in each host
request queue; these are P4 facts, not DWC2-wide constants. Its indexed errata
was unavailable in this review, and section 47.5 omits register details. The
retained P4 core ID is ``0x4f54400a``; the generic Synopsys documents available
here are revision 4.20a. Do not assume exact revision equivalence. [P4]_

The bounded STM32F723 audit checked RM0431 and ES0360. ES0360 Rev 5 lists no
USB-specific limitation, but its write-through D-cache erratum remains a
conditional integration obligation. This target was used for builds and size
comparison; its physical silicon revision, core ID, and cache configuration
were not verified. Other integrations still need their own audit. [F7]_

Baseline policy and unresolved gaps
-----------------------------------

These choices are **not** protocol requirements merely because they exist in
``hcd_dwc2.c`` or ``hub_iso_split_slot()``:

.. list-table::
   :header-rows: 1
   :widths: 32 68

   * - Baseline choice
     - Consequence for a rewrite
   * - Four interrupt budget slots; placement load assumes service every frame
     - Conservative policy. Replace with a proved admission plan or preserve
       explicitly; it is not complete HS/FS bandwidth admission. Actual
       transaction scheduling still tracks endpoint intervals.
   * - ISO IN <=564 bytes; ISO OUT <=1023 bytes; no general frame-spanning plan
     - The IN cap is a chosen subset. USB permits FS ISO packets up to 1023
       bytes. Expansion needs the complete U08 schedule, memory, and bus budget.
   * - Two 192-byte DMA ISO-IN staging buffers
     - Resource policy. Reuse only after DMA ownership ends; capacity failure
       needs a defined endpoint-open result.
   * - Fixed ``DWC2_PERIODIC_OUT_MIN_FRREM=1024``
     - Timing heuristic requiring integration-specific justification.
   * - Terminal interrupt CS NYET not counted as an error
     - Deliberate divergence from USB 11.18.8 case 4; decide and test the intended
       error policy instead of inheriting it invisibly.
   * - MDATA continuation helper only for ISO IN
     - The retained interrupt-IN MDATA failure predates the reviewed PR.
       A complete rewrite must address U15, not preserve that limitation as truth.
   * - DMA SS sequence assigned at enable
     - Ordering proof remains open because the guide describes arbiter admission.
   * - Possible-current scan plus global pending RX/HC predicate
     - The narrow fix passed focused tests, but it can still conservatively defer
       when unrelated IRQ work coincides with another current periodic candidate.
   * - Multiple wait/behind/pass/kicked flags and repeated channel scans
     - Candidate simplification: explicit transaction phases and one admission
       function, not a new flag for each interrupt interleaving.

No claim is made here that every prior microphone tail, restart, or HID gap
has the same internal cause. Likewise, short terminal DATA0 is legal; a
workaround that rejects all short ISO packets would violate the intended model.

A smaller design to evaluate
----------------------------

This is a proposal, not proven replacement code.

1. **Build a plan at endpoint-open time.** Resolve TT identity and reserve
   bus time/resources. Generate legal SS positions and CS opportunity sets
   from USB 11.18.4, including frame identity. Either support the full rule
   set or declare/reject a constrained subset. Runtime need not rediscover
   placement policy in every interrupt.
2. **Use an explicit transaction state machine.** A small set such as
   WAIT_INTERVAL, SS_READY, SS_ACTIVE, CS_WAIT, CS_ACTIVE, ISO_OUT_ACTIVE,
   QUIESCING, and DONE owns phase transitions. ISO OUT has no CS branch.
   Cancellation and hardware-channel ownership remain explicit rather than
   encoded in combinations of several scheduling flags.
3. **Normalize hardware events.** Slave RX/HC and buffer-DMA halt handling
   produce events such as SS_PROGRESS, CS_NOT_READY, RX_CONTINUE, RX_FINAL,
   OUT_COMPLETE, ERROR, and HALTED. Decode raw bits and byte counts once.
   Preserve RX-to-live-HC processing in the same IRQ.
4. **Centralize admission, not all interrupt work.** One bounded function chooses
   legal current actions before future actions, respects per-TT dependencies,
   and checks shared queue/FIFO resources. All periodic producers use it,
   including native-HS retries and ISO OUT continuations. SOF can still update
   time and admit safe urgent work promptly. Do not unconditionally postpone
   every future request until after unrelated control RX has drained.
5. **Keep a small mode adapter.** Slave IN/ZLP, slave nonempty OUT, and DMA
   expose distinct admission/completion semantics. A failed admission leaves
   the action ready without advancing its order, offset, or deadline. An
   accepted action records the information needed to prove the wire schedule.

Illustrative transition rules::

   interval due       -> SS_READY
   admitted SS        -> SS_ACTIVE (record actual admission contract)
   SS progress        -> CS_WAIT, or next ISO OUT piece
   legal CS slot      -> CS_ACTIVE
   CS NYET / MDATA    -> next legal CS slot (MDATA also appends data)
   terminal response  -> DONE or next periodic interval, by response/type
   expired ISO slot   -> DONE(error), never replay or splice
   abort/close        -> QUIESCING -> release only after ownership ends

Candidate reductions:

* ``csplit_wait``, ``_hcd_csplit_behind``, ``_hcd_csplit_pass``, and
  ``_hcd_csplit_late_held`` can potentially become derived readiness/dependency
  state consumed by the one admission pass.
* ``iso_open`` and ``split_held`` can potentially become one TT packet owner
  plus ordinary pending actions.
* Slave FIFO order recording and DMA arbitration stay in the adapter. Removing
  that distinction would recreate H2 or introduce an unproved DMA equivalent.
* Keep bounded arrays appropriate to the controller's small channel count.
  A general dynamic scheduler, timer wheel, or heap is not required merely to
  express the protocol.

The simplification test is whether every state and exception has one owner,
one source requirement, and one test. It is not whether all retries can be
reduced to one common counter or all transactions to the same phase sequence.

Validation required before replacing the scheduler
--------------------------------------------------

Use a pure state/schedule model for exhaustive event-order tests, then an
integrated whole-driver model, then real hardware. A register mock cannot prove
the peripheral's queue arbitration, interrupt exposure, or wire timing.

.. list-table:: Acceptance matrix
   :header-rows: 1
   :widths: 25 48 27

   * - Dimension
     - Required cases
     - Evidence
   * - Schedule boundaries
     - Every supported SS slot; forbidden Y6; Y7 to next frame; U07/U08
       boundary rules; timer wrap; long intervals; missed/coalesced SOFs
     - Model plus HS wire capture
   * - TT topology
     - Real single-TT, multi-TT same/different ports, nested FS hub, multiple HS
       hubs, simultaneous TTs, topology/alternate-setting changes
     - Descriptors plus wire route/order
   * - Per-TT order
     - IN/OUT mixes; SS order opposite channel order; deferred FIFO writes;
       older ACK/NYET/MDATA pending; abort/error of older work; order wrap/reuse
     - Original H2 and permutation tests
   * - Shared periodic queue
     - Current CS versus future CS/ISO OUT/native-HS request; simultaneous
       SOF/RX/HC; RX with and without generated HCINT; unrelated control flood
     - Original H1 plus startup regression
   * - IN assembly
     - ISO and interrupt MDATA; 1/short/188-byte fragments; terminal short/ZLP;
       final-slot MDATA/NYET; missing middle; buffer boundary/overflow;
       interrupt MDATA followed by ERR/CRC failure or terminal toggle mismatch;
       full ISO packets at 564/565/1023 bytes and schedules requiring up to 8 CS
     - Payload sequences and terminal PID
   * - ISO OUT
     - 0/1/187/188/189/376/564/1023 bytes; ALL/BEGIN/MID/END; FIFO pressure;
       missing/late piece; other periodic starts and CS on same/different TT
     - Downstream payload plus HS positions
   * - Error recovery
     - SS/CS errors, ERR versus XactErr, NAK, NYET, STALL, babble, toggle error,
       immediate retries, terminal-window policy, CLEAR_TT_BUFFER
     - State transitions plus fault injection
   * - Memory and modes
     - Slave, buffer DMA, DMA arbitration contention, aligned/unaligned fragment
       totals, cache on/off, finite staging-buffer exhaustion, build guards
     - Target memory and payload checks
   * - Lifecycle
     - Close/abort/reset/unplug at every phase, late RX/halt, channel reuse,
       callback resubmission, suspend/resume, resource rollback
     - Exactly-once completion/ownership checks
   * - Bandwidth/resources
     - HS 80% and TT 90% boundaries, think-time values, mixed intervals,
       failed admission, 16 SS/TT limit, queue-full, FIFO-full, channel shortage
     - Admission model plus saturated workloads
   * - Timing
     - SOF entry-to-service distribution, RX-to-HC completion within one IRQ,
       worst ISR duration and masked time under all mixed workloads
     - DSLogic GPIO2-8/23 plus USBPV
   * - Regression/portability
     - All 26 retained Claude cases on the replacement; matched baseline runs;
       default and explicit DMA builds; relevant MCU manual/errata checks
     - Full receipts tied to source/firmware

Do not write to the disk during these retained mixed workloads: its role is
read-only traffic. Require complete captures and separate analyzer loss from
host service omissions and application payload loss.

Evidence already retained for the committed baseline
----------------------------------------------------

The code commit includes the two ordering corrections and nine whole-driver
regression cases. Existing verification is associated with the committed blobs
(only Git CRLF-to-LF normalization in the two test configuration files):

* 288 unit tests, 21 full board example sweeps, and two explicit DMA builds.
* H1: induced whole-IRQ delay; current B before future A; 845 contiguous B reports.
* H2: native 896-byte FIFO-pressure workload; correct B/A order; 24 intact reports.
* Natural microphone, 30 seconds: no corrupt samples/restart; 32 nominal service
  omissions remain. Natural HID, 60 seconds: six streams, no measured gaps.
* H1-only tracing showed repeated deferral during unrelated control work,
  CHENA within intended u3, and wire CS at u5. This refuted a software
  HFNUM-boundary-crossing explanation for that event.

These are bounded observations, not endurance, full-matrix, DMA-ordering, or
new IRQ-jitter qualification. The 26-case matrix ran on the previous broad
guard, not this narrower guard; it must not be relabeled. Raw Windows scope
and full pre-commit submission gates remain open. Original firmware was restored.

Local review artifacts (outside Git) are under
``.review-artifacts/pr-4195-original-issues/`` in the primary checkout:
``startup-investigation/investigation-result.json``,
``startup-investigation/independent-investigation-summary.json``, and
``spec-review/independent-commit-association.json``. [EVIDENCE]_

Sources, checked pages, and open documentation
----------------------------------------------

Page numbers below are **one-based physical PDF pages**; USB printed page
numbers are 28 lower. The local Calibre IDs and read-doc lookup IDs allow the
same source pages to be reopened. Requirements paraphrase the sources; no
vendor document is embedded in this repository.

.. [U-ROUTE] USB 2.0, Calibre 1162, sections 8.4.2.2-8.4.2.3,
   PDF pp.230-232; lookup ``a8d72174d0b4``. Hub/port, speed/type,
   start/complete token semantics, and correct port even for single-TT hubs.

.. [U-ENDPOINT] USB-IF, *Universal Serial Bus Specification Revision 2.0*,
   Calibre 1162, sections 5.6.4, 5.7.4, 9.6.6; PDF pp.75, 77, 79, 299.
   Lookup ``7cd4c506e75a``. Interval and maximum-packet descriptor semantics.

.. [U-BANDWIDTH] USB 2.0, Calibre 1162, sections 5.6.4, 5.7.4, 5.10,
   5.11.3; PDF pp.75, 77, 86, 91-92. Lookups ``40ad1d5d4ccb``,
   ``98a7b1054288``. Bus-time admission and HS split allocation.

.. [U-BUDGET] USB 2.0, Calibre 1162, sections 11.18.1-11.18.2,
   PDF p.401; lookup ``84ed67efecf7``. Best-case budget, maximum payload,
   interpacket/turnaround and TT think time.

.. [U-SCHEDULE] USB 2.0, Calibre 1162, section 11.18.4,
   PDF pp.402-406, Figures 11-61 through 11-65;
   lookup ``52e3b6a69068``. Scheduling and immediate-retry rules.

.. [U-RESP] USB 2.0, Calibre 1162, sections 11.18.5 and 11.20,
   PDF pp.406-407, 410-422; lookup ``80f8dade3011``.
   TT response generation, interrupt split state machines and fragment rollback.

.. [U-PIPELINE] USB 2.0, Calibre 1162, sections 11.18.6-11.18.8,
   PDF pp.407-410. Reached from ``52e3b6a69068`` and ``80f8dade3011``;
   pipeline lifetime, order, and last-CS NYET policy.

.. [U-ISO] USB 2.0, Calibre 1162, section 11.21,
   PDF pp.423-429, particularly Figures 11-83, 11-84, 11-89;
   lookup ``0ec4a81974e8``. ISO phases, no wire SS ACK, and final PID/error.

.. [U-NONPERIODIC] USB 2.0, Calibre 1162, sections 11.17 and 11.24.2.3,
   PDF pp.388-400, 451-452; lookup ``9a847a4f4eb6``. Nonperiodic split state
   and TT buffer cleanup. Native-HS PING: DWC2 guide section 3.6, PDF p.74.

.. [D-ARCH] Synopsys, *USB 2.0 Hi-Speed On-The-Go (OTG) Databook*,
   revision 4.20a, July 2020, Calibre 1222, PDF pp.39-40, 65, 82;
   lookups ``0ca8a4ff54ab``, ``eb2fd1ee4291``. Modes and host architecture.

.. [D-REG] DWC OTG Databook 4.20a, Calibre 1222,
   tables 7-40, 7-45, 7-46, PDF pp.485-486, 500-508;
   lookups ``0ca8a4ff54ab``, ``eb2fd1ee4291``. OddFrm, EC and HCSPLT
   tables visually checked. Queue-top fields are diagnostic; an unchanged
   HPTXSTS value alone is not proof of an active blocking request.

.. [D-FIFO] Synopsys, *USB 2.0 Hi-Speed On-The-Go (OTG) Programming Guide*,
   revision 4.20a, July 2020, Calibre 1224 (library title says
   "DWC otg programming changes"; PDF title page identifies the guide).
   Table 1-2 and sections 3.7-3.7.1, PDF pp.29, 75;
   lookup ``af9f44ce6075``. Table 1-2 visually checked.

.. [D-EVENT] DWC OTG Databook 4.20a, Calibre 1222, sections 7.1.9,
   7.1.42, PDF pp.384-385, 509-514. Lookup ``1313db26b360`` and pages
   adjacent to [D-REG]_. Receive pop and HCINT tables visually checked.

.. [D-XFER] DWC OTG Databook 4.20a, Calibre 1222, section 7.1.44,
   PDF pp.519-521; lookup ``883611a9fee2``. HCTSIZ direction-specific
   meaning, PID, packet count, and DoPng. Size table visually checked.

.. [D-HALT] DWC OTG Programming Guide 4.20a, Calibre 1224,
   sections 3.4-3.6, PDF pp.72-74; lookup ``ced9c3810403``.
   Halt protocol and mode-specific restrictions visually checked on p.73.

.. [D-SLAVE] DWC OTG Programming Guide 4.20a, Calibre 1224,
   sections 4.1.3-4.1.4, 4.2.2, 4.2.4, 4.3.3-4.3.4,
   PDF pp.89-92, 96-103, 108-112; lookup ``faf1565c7951``.

.. [D-DMA] DWC OTG Programming Guide 4.20a, Calibre 1224,
   sections 5.1.3-5.1.4, 5.2.3-5.2.4, 5.3.3-5.3.4,
   PDF pp.122-125, 132-145; lookups ``faf1565c7951``, ``4d6b6e17b9cb``.
   In particular p.132 states request admission on arbiter grant.

.. [P4] Espressif, *ESP32-P4 Chip Revision v1.3 Technical Reference Manual*,
   pre-release v0.2, Calibre 491, chapter 47, PDF pp.2432-2443;
   lookup ``6cd33563b5a9``. Integration limits above are verified against
   sections 47.3.3 and 47.4.3.1. Final-DWORD admission and the register-detail
   limitation were checked on pp.2442-2443, lookup ``7f69e0e6260c``.
   Errata search ``7497476e09fe`` found no indexed match; errata coverage is
   **unavailable**, not verified absent.

.. [F7] STMicroelectronics, *STM32F72xxx and STM32F73xxx reference manual*,
   RM0431 Rev 4, Calibre 760, PDF pp.1243-1248, 1251, 1309-1311, 1315-1317;
   lookups ``96958c51ff8a`` and ``342a738d9278``. Host frame/parity, channel
   enable/halt, and receive/channel interrupt programming were checked.
   *STM32F72xxx and STM32F73xxx device limitations*, ES0360 Rev 5,
   Calibre 755, PDF pp.1-7; lookups ``4428983c674c``, ``aedbf40b25bb``,
   ``176371cfb1b0``, and ``c11fb22726ef``. Applicability lists revisions A/1;
   section 2.1.1 describes Arm write-through-cache erratum 1259864.

.. [EVIDENCE] Review artifacts for PR #4195 and local fixes, 2026-10-19:
   original H1/H2 reproductions, ISO OUT interleaving comparison, H1-only/H2-only
   isolation, RAM trace, four scoped-candidate comparisons, and independent
   commit association. Findings and evidence are specific to their source and
   firmware hashes; see the local artifact index above.

The Linux DWC2 implementation is useful as a comparison, not a normative
substitute for these sources: `hcd_intr.c
<https://github.com/torvalds/linux/blob/master/drivers/usb/dwc2/hcd_intr.c>`_.
This study does not copy its scheduler or claim that all of its workarounds
are necessary for TinyUSB.
