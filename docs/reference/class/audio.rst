*****
Audio
*****

Role: device only.  TinyUSB supports USB Audio Class 1.0 and 2.0 streaming.
The descriptors define the topology, formats, channels, rates, controls, and
alternate settings; the application supplies or consumes the audio samples.

Start from an example
=====================

Audio descriptors and buffer sizes are tightly coupled.  Copy the closest
example, confirm that it enumerates, and then change one property at a time:

* :doc:`../../examples/device/audio_test` -- one-channel UAC2 microphone;
* :doc:`../../examples/device/audio_4_channel_mic` -- four-channel microphone;
* :doc:`../../examples/device/uac2_speaker_fb` -- speaker with feedback;
* :doc:`../../examples/device/uac2_headset` -- bidirectional headset;
* :doc:`../../examples/device/audio_test_multi_rate` -- UAC1 at full speed,
  UAC2 at high speed, with multiple rates.

Configuration
=============

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
=========

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
================

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
