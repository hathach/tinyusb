# audio device: short EP IN FIFO read replays stale bytes

Split out of PR #3851 (found by the hfp HIL rig on stm32f746disco-DMA
audio_test_freertos: "Audio mismatch at sample 8702: expected 8702, got 8696").

## Established

- `src/class/audio/audio_device.c` `audiod_tx_xfer_isr()` (~line 527): the return
  of `tu_fifo_read_n(ff, lin_buf_in, n_bytes_tx)` is ignored and
  `usbd_edpt_xfer()` still submits `n_bytes_tx`. When the SW FIFO holds fewer
  bytes than the flow-control packet size, the tail of `lin_buf_in` is the
  previous packet's data — the host hears a replayed fragment, offset by exactly
  one packet (6 samples at HS 48 kHz mono 16-bit: "expected 8702, got 8696").
- Reproduced whenever the producer lets the FIFO underrun; the example was fixed
  to hold the FIFO at threshold (PR #3851, `20ff2b498`), but any application
  with >~2 ms producer starvation can still hit the driver bug.
- A naive short-write is not a fix: sending fewer bytes than the flow-control
  size is legal ISO behavior, but a 1-byte pad would byte-misalign the stream —
  the packet must shrink to the bytes actually read (sample-aligned).

## Remains

- In `audiod_tx_xfer_isr()`, submit the number of bytes `tu_fifo_read_n()`
  actually returned (rounded down to a whole sample frame), not `n_bytes_tx`;
  audit the flow-control accounting (`ctrl_blackout`, byte budget) for the
  shrunken packet so long-term rate stays correct.
- Coverage: hfp rig audio_test_freertos with a deliberately starved producer;
  unit test on the FIFO-short path if feasible.

## Why split

PR #3851 is a board/clock PR; this is an audio class-driver correctness fix
with its own review and test surface.
