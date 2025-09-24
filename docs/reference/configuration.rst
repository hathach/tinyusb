*************
Configuration
*************

TinyUSB behavior is controlled through compile-time configuration in ``tusb_config.h``. This reference covers all available configuration options.

Basic Configuration
===================

Required Settings
-----------------

.. code-block:: c

   // Target MCU family - REQUIRED
   #define CFG_TUSB_MCU                OPT_MCU_STM32F4

   // OS abstraction layer - REQUIRED
   #define CFG_TUSB_OS                 OPT_OS_NONE

   // Enable device or host stack
   #define CFG_TUD_ENABLED             1  // Device stack
   #define CFG_TUH_ENABLED             1  // Host stack

Debug and Logging
-----------------

.. code-block:: c

   // Debug level (0=off, 1=error, 2=warning, 3=info)
   #define CFG_TUSB_DEBUG              2

   // Memory alignment for buffers (usually 4)
   #define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))

Device Stack Configuration
==========================

Endpoint Configuration
----------------------

.. code-block:: c

   // Control endpoint buffer size
   #define CFG_TUD_ENDPOINT0_SIZE      64

   // Number of endpoints (excluding EP0)
   #define CFG_TUD_ENDPOINT_MAX        16

Device Classes
--------------

**CDC (Communication Device Class)**:

.. code-block:: c

   #define CFG_TUD_CDC                 1    // Number of CDC interfaces
   #define CFG_TUD_CDC_EP_BUFSIZE      512  // CDC endpoint buffer size
   #define CFG_TUD_CDC_RX_BUFSIZE      256  // CDC RX FIFO size
   #define CFG_TUD_CDC_TX_BUFSIZE      256  // CDC TX FIFO size

**HID (Human Interface Device)**:

.. code-block:: c

   #define CFG_TUD_HID                 1    // Number of HID interfaces
   #define CFG_TUD_HID_EP_BUFSIZE      16   // HID endpoint buffer size

**MSC (Mass Storage Class)**:

.. code-block:: c

   #define CFG_TUD_MSC                 1    // Number of MSC interfaces
   #define CFG_TUD_MSC_EP_BUFSIZE      512  // MSC endpoint buffer size

**Audio Class**:

.. code-block:: c

   #define CFG_TUD_AUDIO               1    // Number of audio interfaces
   #define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                 220
   #define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                 1
   #define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ             64
   #define CFG_TUD_AUDIO_ENABLE_EP_IN                    1
   #define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX   2
   #define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            2

**MIDI**:

.. code-block:: c

   #define CFG_TUD_MIDI                1    // Number of MIDI interfaces
   #define CFG_TUD_MIDI_RX_BUFSIZE     128  // MIDI RX buffer size
   #define CFG_TUD_MIDI_TX_BUFSIZE     128  // MIDI TX buffer size

**DFU (Device Firmware Update)**:

.. code-block:: c

   #define CFG_TUD_DFU                 1    // Enable DFU mode
   #define CFG_TUD_DFU_XFER_BUFSIZE    512  // DFU transfer buffer size

**Vendor Class**:

.. code-block:: c

   #define CFG_TUD_VENDOR              1    // Number of vendor interfaces
   #define CFG_TUD_VENDOR_EPSIZE       64   // Vendor endpoint size
   #define CFG_TUD_VENDOR_RX_BUFSIZE   64   // RX buffer size (0 = no buffering)
   #define CFG_TUD_VENDOR_TX_BUFSIZE   64   // TX buffer size (0 = no buffering)

.. note::
   Unlike other classes, vendor class supports setting buffer sizes to 0 to disable internal buffering. When disabled, data goes directly to ``tud_vendor_rx_cb()`` and the ``tud_vendor_read()``/``tud_vendor_write()`` functions are not available - applications must handle data directly in callbacks.

Host Stack Configuration
========================

Port and Hub Configuration
--------------------------

.. code-block:: c

   // Number of host root hub ports
   #define CFG_TUH_HUB                 1

   // Number of connected devices (including hub)
   #define CFG_TUH_DEVICE_MAX          5

   // Control transfer buffer size
   #define CFG_TUH_ENUMERATION_BUFSIZE 512

Host Classes
------------

**CDC Host**:

.. code-block:: c

   #define CFG_TUH_CDC                 2    // Number of CDC host instances
   #define CFG_TUH_CDC_FTDI            1    // FTDI serial support
   #define CFG_TUH_CDC_CP210X          1    // CP210x serial support
   #define CFG_TUH_CDC_CH34X           1    // CH34x serial support

**HID Host**:

.. code-block:: c

   #define CFG_TUH_HID                 4    // Number of HID instances
   #define CFG_TUH_HID_EPIN_BUFSIZE    64   // HID endpoint buffer size
   #define CFG_TUH_HID_EPOUT_BUFSIZE   64

**MSC Host**:

.. code-block:: c

   #define CFG_TUH_MSC                 1    // Number of MSC instances
   #define CFG_TUH_MSC_MAXLUN          4    // Max LUNs per device

Advanced Configuration
======================

Memory Management
-----------------

.. code-block:: c

   // Enable stack protection
   #define CFG_TUSB_DEBUG_PRINTF       printf

   // Custom memory allocation (if needed)
   #define CFG_TUSB_MEM_SECTION        __attribute__((section(".usb_ram")))

RTOS Configuration
------------------

TinyUSB supports multiple operating systems through its OSAL (Operating System Abstraction Layer). Choose the appropriate configuration based on your target environment.

**FreeRTOS Integration**:

When using FreeRTOS, configure the task queue sizes to handle USB events efficiently:

.. code-block:: c

   #define CFG_TUSB_OS                 OPT_OS_FREERTOS
   #define CFG_TUD_TASK_QUEUE_SZ       16  // Device task queue size
   #define CFG_TUH_TASK_QUEUE_SZ       16  // Host task queue size

**RT-Thread Integration**:

RT-Thread requires only the OS selection, as it uses the RTOS's built-in primitives:

.. code-block:: c

   #define CFG_TUSB_OS                 OPT_OS_RTTHREAD

Low Power Configuration
-----------------------

.. code-block:: c

   // Enable remote wakeup
   #define CFG_TUD_USBD_ENABLE_REMOTE_WAKEUP  1

   // Suspend/resume callbacks
   // Implement tud_suspend_cb() and tud_resume_cb()

MCU-Specific Options
====================

The ``CFG_TUSB_MCU`` option selects the target microcontroller family:

.. code-block:: c

   // STM32 families
   #define CFG_TUSB_MCU    OPT_MCU_STM32F0
   #define CFG_TUSB_MCU    OPT_MCU_STM32F1
   #define CFG_TUSB_MCU    OPT_MCU_STM32F4
   #define CFG_TUSB_MCU    OPT_MCU_STM32F7
   #define CFG_TUSB_MCU    OPT_MCU_STM32H7

   // NXP families
   #define CFG_TUSB_MCU    OPT_MCU_LPC18XX
   #define CFG_TUSB_MCU    OPT_MCU_LPC40XX
   #define CFG_TUSB_MCU    OPT_MCU_LPC43XX
   #define CFG_TUSB_MCU    OPT_MCU_KINETIS_KL
   #define CFG_TUSB_MCU    OPT_MCU_IMXRT

   // Other vendors
   #define CFG_TUSB_MCU    OPT_MCU_RP2040
   #define CFG_TUSB_MCU    OPT_MCU_ESP32S2
   #define CFG_TUSB_MCU    OPT_MCU_ESP32S3
   #define CFG_TUSB_MCU    OPT_MCU_SAMD21
   #define CFG_TUSB_MCU    OPT_MCU_SAMD51
   #define CFG_TUSB_MCU    OPT_MCU_NRF5X

Configuration Examples
======================

Minimal Device (CDC only)
--------------------------

.. code-block:: c

   #define CFG_TUSB_MCU                OPT_MCU_STM32F4
   #define CFG_TUSB_OS                 OPT_OS_NONE
   #define CFG_TUSB_DEBUG              0

   #define CFG_TUD_ENABLED             1
   #define CFG_TUD_ENDPOINT0_SIZE      64

   #define CFG_TUD_CDC                 1
   #define CFG_TUD_CDC_EP_BUFSIZE      512
   #define CFG_TUD_CDC_RX_BUFSIZE      512
   #define CFG_TUD_CDC_TX_BUFSIZE      512

   // Disable other classes
   #define CFG_TUD_HID                 0
   #define CFG_TUD_MSC                 0
   #define CFG_TUD_MIDI                0
   #define CFG_TUD_AUDIO               0
   #define CFG_TUD_VENDOR              0

Full-Featured Host
------------------

.. code-block:: c

   #define CFG_TUSB_MCU                OPT_MCU_STM32F4
   #define CFG_TUSB_OS                 OPT_OS_FREERTOS
   #define CFG_TUSB_DEBUG              2

   #define CFG_TUH_ENABLED             1
   #define CFG_TUH_HUB                 1
   #define CFG_TUH_DEVICE_MAX          8
   #define CFG_TUH_ENUMERATION_BUFSIZE 512

   #define CFG_TUH_CDC                 2
   #define CFG_TUH_HID                 4
   #define CFG_TUH_MSC                 2
   #define CFG_TUH_VENDOR              2

Validation
==========

Use these checks to validate your configuration:

.. code-block:: c

   // In your main.c, add compile-time checks
   #if !defined(CFG_TUSB_MCU) || (CFG_TUSB_MCU == OPT_MCU_NONE)
   #error "CFG_TUSB_MCU must be defined"
   #endif

   #if CFG_TUD_ENABLED && !defined(CFG_TUD_ENDPOINT0_SIZE)
   #error "CFG_TUD_ENDPOINT0_SIZE must be defined for device stack"
   #endif

Common Configuration Issues
===========================

1. **Endpoint buffer size too small**: Causes transfer failures
2. **Missing CFG_TUSB_MCU**: Build will fail
3. **Incorrect OS setting**: RTOS functions won't work properly
4. **Insufficient endpoint count**: Device enumeration will fail
5. **Buffer size mismatches**: Data corruption or transfer failures

For configuration examples specific to your board, check ``examples/device/*/tusb_config.h``.