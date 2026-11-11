*****
Video
*****

Role: device only.  The USB Video Class (UVC) driver streams application-owned
video frames and handles the standard probe/commit negotiation used by host
camera software.

Start from an example
=====================

UVC descriptors contain a linked control topology plus one or more formats,
frames, intervals, and streaming alternate settings.  Start from
:doc:`../../examples/device/video_capture` and change the format or dimensions
incrementally.  Use :doc:`../../examples/device/video_capture_2ch` for multiple
control/streaming functions.

Configuration
=============

``CFG_TUD_VIDEO`` counts VideoControl interfaces and
``CFG_TUD_VIDEO_STREAMING`` counts VideoStreaming interfaces.  Set
``CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE`` to at least the payload size used by the
stream.  The examples use ``CFG_TUD_VIDEO_STREAMING_BULK`` to choose bulk or
isochronous descriptors; the endpoint type in those descriptors is what the
driver follows.

.. list-table::
   :header-rows: 1
   :widths: 43 16 41

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_VIDEO``
     - ``0``
     - Number of VideoControl functions retained by the driver.
   * - ``CFG_TUD_VIDEO_STREAMING``
     - Required
     - Total VideoStreaming interfaces across all control functions.
   * - ``CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE``
     - Required
     - Per-stream USB payload buffer, including the UVC payload header.  If it
       is smaller than the negotiated payload, TinyUSB caps each transfer to
       this size.
   * - ``CFG_TUD_VIDEO_STREAMING_BULK``
     - Example-defined
     - Chooses between the example's bulk and isochronous descriptor layouts;
       it is not interpreted by the class driver itself.

The helpers in ``src/class/video/video.h`` build individual UVC descriptor
blocks; unlike simpler classes, there is no single descriptor macro for every
camera topology.  Verify entity IDs, terminal links, class-specific total
lengths, format/frame counts, endpoint addresses, and alternate settings as a
unit.

Frame flow
==========

.. list-table::
   :header-rows: 1
   :widths: 43 57

   * - API or callback
     - What it does
   * - ``tud_video_n_connected()``
     - Tests whether a VideoControl function is mounted.
   * - ``tud_video_n_streaming()``
     - Tests whether the host selected an active streaming alternate setting
       for a control/stream pair.
   * - ``tud_video_n_frame_xfer()``
     - Queues one non-empty frame.  ``false`` means no active endpoint, probe is
       in progress, another frame is still owned by the driver, the endpoint is
       halted, or it could not accept the first payload.  No callback follows a
       refused frame.
   * - ``tud_video_frame_xfer_complete_cb()``
     - Releases the queued frame after all of its UVC payloads complete.  The
       default abort callback also forwards here for legacy applications.
   * - ``tud_video_frame_xfer_abort_cb()``
     - Releases an accepted frame aborted by COMMIT, endpoint halt, or
       transfer failure.  This does not report delivery of a complete frame.
       Its default weak implementation calls
       ``tud_video_frame_xfer_complete_cb()``.  SET_INTERFACE, bus reset and
       unconfiguration drop a queued frame with no callback.
   * - ``tud_video_commit_cb()``
     - Validates and applies the host's committed format, frame index, and
       interval; return a ``video_error_code_t`` value.
   * - ``tud_video_power_mode_cb()``
     - Applies a host power-mode control request or returns an appropriate UVC
       error.
   * - ``tud_video_prepare_payload_cb()``
     - Fills payload bytes on demand when the frame was queued with a null data
       pointer; honor the requested offset and maximum length.  The first call
       runs inside ``tud_video_n_frame_xfer()`` in the caller's context, later
       ones from ``tud_task()``.  Do not call ``tud_video_n_frame_xfer()`` from
       it: the submit path holds the class lock while calling it.

Wait for ``tud_video_n_streaming(ctl_idx, stm_idx)`` before submitting a frame.
Queue it with ``tud_video_n_frame_xfer()`` and do not modify or reuse the buffer
until ``tud_video_frame_xfer_complete_cb()`` or
``tud_video_frame_xfer_abort_cb()``.  Override the latter to distinguish
an abort from full-frame completion.  A COMMIT keeps the queued payload and
buffer ownership until that payload drains; if it was the final payload and
succeeds, the frame completes normally.  An endpoint halt, refused continuation,
or a COMMIT after the payload completion was lost releases the frame without
waiting for a completion that will not arrive.

``tud_video_n_streaming()`` turning false during PROBE does not release a
queued frame: the driver still owns it and keeps sending it.  Bus reset and
unconfiguration drop a queued frame with no callback; the host configures the
device again before streaming resumes, so reset the application's frame state in
``tud_mount_cb()``.

SET_INTERFACE also drops a queued frame with no callback, and on a bulk
interface or a switch between non-zero alternate settings
``tud_video_n_streaming()`` stays true.  The application cannot tell this from a
slow host, so do not rewrite a buffer the driver may still own, for example on a
timeout.  ``tud_video_n_frame_xfer()`` refuses a frame while the driver owns
one, so once it accepts a new frame the previous buffer is free.  On some ports
(`#4428 <https://github.com/hathach/tinyusb/issues/4428>`_, e.g. rp2040) the
endpoint stays busy and every submit is refused until a bus reset or a change
of configuration (a repeated SET_CONFIGURATION with the same value is ignored);
recover in ``tud_mount_cb()``.

Implement ``tud_video_commit_cb()`` to inspect and adopt the format, frame, and
interval committed by the host.  Generate frames at the negotiated interval;
continuing at a hard-coded rate can overflow or starve the stream.

For data generated directly into USB payloads, submit a null frame buffer with
the intended frame size and fill each request in
``tud_video_prepare_payload_cb()``.  Respect the supplied length and offset and
avoid lengthy work in the callback.

Isochronous endpoints reserve periodic bandwidth and tolerate a missed packet;
bulk endpoints retry errors but provide no bandwidth guarantee.  Check that the
advertised maximum packet size is feasible for the controller and bus speed.

Specification used: *USB Device Class Definition for Video Devices*, Revision
1.5.
