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
     - ``0``
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
       in progress, or another frame is still owned by the driver.
   * - ``tud_video_frame_xfer_complete_cb()``
     - Releases the queued frame after all of its UVC payloads complete.
   * - ``tud_video_commit_cb()``
     - Validates and applies the host's committed format, frame index, and
       interval; return a ``video_error_code_t`` value.
   * - ``tud_video_power_mode_cb()``
     - Applies a host power-mode control request or returns an appropriate UVC
       error.
   * - ``tud_video_prepare_payload_cb()``
     - Fills payload bytes on demand when the frame was queued with a null data
       pointer; honor the requested offset and maximum length.

Wait for ``tud_video_n_streaming(ctl_idx, stm_idx)`` before submitting a frame.
Queue it with ``tud_video_n_frame_xfer()`` and do not modify or reuse the buffer
until ``tud_video_frame_xfer_complete_cb()``.

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
