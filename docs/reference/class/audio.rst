*****
Audio
*****

TinyUSB supports USB Audio Class 1.0 and 2.0 streaming in device and host
roles.

Device driver
=============

The descriptors define the topology, formats, channels, rates, controls, and
alternate settings; the application supplies or consumes the audio samples.

Start from an example
---------------------

Audio descriptors and buffer sizes are tightly coupled.  Copy the closest
example, confirm that it enumerates, and then change one property at a time:

* :doc:`../../examples/device/audio_test` -- one-channel UAC2 microphone;
* :doc:`../../examples/device/audio_4_channel_mic` -- four-channel microphone;
* :doc:`../../examples/device/uac2_speaker_fb` -- speaker with feedback;
* :doc:`../../examples/device/uac2_headset` -- bidirectional headset;
* :doc:`../../examples/device/audio_test_multi_rate` -- UAC1 at full speed,
  UAC2 at high speed, with multiple rates.

Configuration
-------------

Set ``CFG_TUD_AUDIO`` to the number of audio functions.  The principal options
are:

.. list-table::
   :header-rows: 1
   :widths: 37 16 47

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_AUDIO_CTRL_BUF_SZ``
     - ``64`` bytes
     - Largest class control payload, such as a RANGE or channel-cluster
       response.  Increase it to fit the largest advertised control.
   * - ``CFG_TUD_AUDIO_ENABLE_EP_IN``
     - ``0``
     - Enables microphone/device-to-host streaming.
   * - ``CFG_TUD_AUDIO_ENABLE_EP_OUT``
     - ``0``
     - Enables speaker/host-to-device streaming.
   * - ``CFG_TUD_AUDIO_FUNC_n_EP_IN_SZ_MAX`` /
       ``CFG_TUD_AUDIO_FUNC_n_EP_OUT_SZ_MAX``
     - Required per enabled direction
     - Maximum endpoint packet size across that function's alternate settings.
   * - ``CFG_TUD_AUDIO_FUNC_n_EP_IN_SW_BUF_SZ`` /
       ``CFG_TUD_AUDIO_FUNC_n_EP_OUT_SW_BUF_SZ``
     - ``0``
     - Software FIFO size.  Set it to at least the corresponding maximum
       endpoint size when using the FIFO APIs.
   * - ``CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL``
     - ``1``
     - Adapts IN packet consumption to the FIFO fill level to reduce
       underruns/overruns.
   * - ``CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP``
     - ``0``
     - Enables an explicit feedback endpoint, normally required by an
       asynchronous speaker.
   * - ``CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP``
     - ``0``
     - Enables the AudioControl interrupt endpoint for status notifications.

For each enabled direction, define the maximum endpoint size used by any
advertised alternate setting, for example
``CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX``.  A software FIFO such as
``CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ`` must be at least that large.  Use
``TUD_AUDIO_EP_SIZE()`` as the examples do; high-speed audio has more service
intervals per millisecond than full-speed audio.

Data path
---------

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Operation
     - Main API
   * - ``tud_audio_mounted()`` / ``tud_audio_version()``
     - Tests whether function zero is configured and returns its negotiated
       Audio Class version.
   * - ``tud_audio_available()`` / ``tud_audio_read()``
     - Reports and removes speaker bytes from the OUT software FIFO.  The read
       count can be shorter than requested.
   * - ``tud_audio_write()``
     - Copies microphone bytes into the IN software FIFO and returns the number
       accepted.
   * - ``tud_audio_clear_ep_*_ff()``
     - Discards queued samples in the selected endpoint FIFO, useful when a
       streaming alternate setting closes.
   * - ``tud_audio_get_ep_*_ff()``
     - Returns the underlying FIFO object for advanced zero-copy or DMA
       integration; the application must preserve its invariants.
   * - ``tud_audio_n_fb_set()``
     - Supplies the feedback value for one audio function when application
       feedback mode is used.  Pass 16.16 samples per frame; TinyUSB converts
       it to full-speed 10.14 format when required.
   * - ``tud_audio_feedback_update()``
     - Updates internally calculated feedback from elapsed master-clock cycles
       and returns the current 16.16 value, or zero on error.
   * - ``tud_audio_n_*()``
     - Selects a function explicitly with ``func_id``; helpers without ``n``
       operate on function zero.

The host starts and stops a stream by selecting interface alternate settings.
Use ``tud_audio_set_itf_cb()`` and ``tud_audio_set_itf_close_ep_cb()`` to start
or stop the application-side I2S/DMA path.  Do not produce or consume samples
merely because the device is mounted; wait until the streaming interface is
active.

Control requests
----------------

Implement the ``tud_audio_get_req_*_cb()`` and ``tud_audio_set_req_*_cb()``
callbacks for every control advertised by the descriptors, such as clock
frequency, clock validity, mute, and volume.  A descriptor that advertises a
control but stalls its normal requests is likely to be rejected or behave
poorly on a host.

Asynchronous speakers normally need a feedback endpoint.  Enable
``CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP`` and either configure the feedback method
through ``tud_audio_feedback_params_cb()`` or provide feedback with
``tud_audio_n_fb_set()``.  Begin with the speaker-feedback example; incorrect
feedback causes periodic underruns or overruns even when the nominal sample
rates match.

Specifications used: *USB Device Class Definition for Audio Devices*, Release
1.0 and Release 2.0, plus *Audio Data Formats*, Release 2.0.

Host driver
===========

The host driver discovers one playback stream and one capture stream in each
supported AudioControl function.  It presents each usable alternate setting
and discrete sample rate as a complete ``format``, ``sample_rate``, and
``channels`` configuration.  The application selects one of these tuples;
TinyUSB manages interface alternate settings, endpoints, packet sizing, and
explicit feedback.

Start from :doc:`../../examples/host/audio_host`.  It discovers configurations,
selects signed 16-bit capture and playback streams, and cycles through
microphone, speaker, and loopback operation.

Configuration
-------------

Set ``CFG_TUH_AUDIO`` to enable the driver.  The principal options are:

.. list-table::
   :header-rows: 1
   :widths: 36 18 46

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUH_AUDIO``
     - ``0``
     - Enables the Audio host class.  A nonzero value includes the driver.
   * - ``CFG_TUH_AUDIO_PROTOCOLS``
     - UAC1
     - Bitmask of ``TUH_AUDIO_PROTOCOL_UAC1`` and
       ``TUH_AUDIO_PROTOCOL_UAC2``.  Combine them to accept both versions.
   * - ``CFG_TUH_AUDIO_MAX``
     - ``1``
     - Maximum number of mounted AudioControl functions.
   * - ``CFG_TUH_AUDIO_MAX_AS``
     - ``4``
     - Maximum number of nonzero-bandwidth alternate settings retained per
       logical stream.
   * - ``CFG_TUH_AUDIO_MAX_SAM_FREQ``
     - ``5``
     - Maximum discrete sample rates retained per UAC1 alternate setting or
       UAC2 Clock Source.
   * - ``CFG_TUH_AUDIO_EPIN_BUFSIZE`` /
       ``CFG_TUH_AUDIO_EPOUT_BUFSIZE``
     - ``256`` bytes
     - Largest capture/playback packet the driver can submit.  A configuration
       whose packet for one polling interval is larger is rejected.
   * - ``CFG_TUH_AUDIO_STREAM_BUFSIZE``
     - ``1024`` bytes
     - Per-stream FIFO depth.  Playback sends silence when a complete packet
       is unavailable; capture overwrites the oldest complete frames when the
       FIFO is full.

Increase the endpoint buffers for high channel counts, sample rates, or sample
widths.  All buffers are statically allocated, so these maxima directly affect
RAM use.

Stream lifecycle
----------------

After ``tuh_audio_mount_cb()``, enumerate stream indices from zero through
``tuh_audio_stream_count() - 1``.  Use ``tuh_audio_stream_direction()`` to
distinguish ``TUH_AUDIO_STREAM_PLAYBACK`` (host to device) from
``TUH_AUDIO_STREAM_CAPTURE`` (device to host).  Then enumerate the stream's
configurations with ``tuh_audio_config_count()`` and
``tuh_audio_config_get()``.

For each stream:

1. Call ``tuh_audio_configure()`` with a supported configuration index while
   the stream is stopped.
2. Call ``tuh_audio_start()``.  A ``true`` return means that startup was
   submitted, not that it completed.
3. Wait for ``TUH_AUDIO_EVENT_START_COMPLETE`` in ``tuh_audio_event_cb()`` and
   check that its transfer result is successful before handling samples.
4. Use the non-blocking frame FIFO APIs while the stream runs.
5. Call ``tuh_audio_stop()`` and wait for
   ``TUH_AUDIO_EVENT_STOP_COMPLETE`` before reconfiguring the stream.

Capture and playback streams in the same AudioControl function must use the
same sample rate while they run concurrently.  ``tuh_audio_active_config()``
returns ``TUSB_INDEX_INVALID_8`` when no configuration is selected.

Audio data is counted in frames, not bytes.  One frame contains one sample for
every channel; obtain its byte size with ``tuh_audio_config_frame_size()``.
``tuh_audio_read()`` removes whole frames from a capture FIFO, while
``tuh_audio_write()`` queues whole frames for playback.  Their return values
may be shorter than requested.  Use ``tuh_audio_read_available()`` and
``tuh_audio_write_available()`` to service the FIFOs from the application task;
the transfer callbacks are notifications and need not drive FIFO servicing.

Callbacks and failures
----------------------

``tuh_audio_descriptor_cb()`` runs during enumeration, before the mount
callback.  Its descriptor pointers are valid only during the call.  Copy any
entity IDs or descriptor fields needed for later raw controls, but do not
submit control transfers from this callback.

``tuh_audio_capture_cb()`` and ``tuh_audio_playback_cb()`` report successful
isochronous transfers.  ``tuh_audio_event_cb()`` reports asynchronous start and
stop completion and ``TUH_AUDIO_EVENT_XFER_FAILED``.  A failed stream has been
stopped; the application may reconfigure or restart it.  Clear all saved
indices and associated state in ``tuh_audio_umount_cb()`` because an index may
be reused by a later device.

Run ``tuh_task()`` continuously.  Isochronous transfers follow the endpoint's
polling interval, and delaying the host task can exhaust the FIFO even when its
average producer and consumer rates match.

Feature Unit controls
---------------------

The driver discovers master mute and volume capabilities before the mount
callback.  Test mute with ``tuh_audio_mute_supported()`` and retrieve the
cached volume range with ``tuh_audio_volume_range_get()``.  Volume values use
signed 1/256 dB units; ``TUH_AUDIO_VOLUME_SILENCE`` represents silence, and
``TUH_AUDIO_CHANNEL_MASTER`` selects the master channel.

The asynchronous ``tuh_audio_mute_*()`` and ``tuh_audio_volume_*()`` APIs use
a completion callback.  Their synchronous helpers block until completion and
should only be used while audio streaming is stopped; synchronous transfers
can disrupt isochronous traffic.  Use ``tuh_audio_control_xfer()`` for other
class-specific entity controls.  Buffers passed to asynchronous controls must
remain valid until their completion callbacks run.

Supported formats and limitations
---------------------------------

The host accepts Type-I PCM in signed 8-, 16-, packed 24-, 24-in-32-, and
32-bit little-endian formats.  UAC1 descriptors must list discrete sample
rates; continuous ranges are not supported.  UAC2 requires a directly
connected Clock Source.  Clock Selectors, Clock Multipliers, Sampling Rate
Converters, Clock Validity, and Valid Alternate Settings controls are not
handled.

Explicit feedback endpoints using 10.14 or 16.16 values are supported.
Implicit-feedback IN endpoints are treated as ordinary capture endpoints and
do not pace playback.  ``MaxPacketsOnly`` endpoints are not supported: OUT
packets are not padded to ``wMaxPacketSize``, and padding in IN packets is not
removed from the captured data.
