*********************
Integration Guide
*********************

This guide covers integrating TinyUSB into production projects with your own build system, custom hardware, and specific requirements.

Project Integration Methods
============================

Method 1: Git Submodule (Recommended)
--------------------------------------

Best for projects using git version control.

.. code-block:: bash

   # Add TinyUSB as submodule
   git submodule add https://github.com/hathach/tinyusb.git lib/tinyusb
   git submodule update --init --recursive

**Advantages:**
- Pinned to specific TinyUSB version
- Easy to update with ``git submodule update``
- Version control tracks exact TinyUSB commit

Method 2: Package Manager Integration
-------------------------------------

**PlatformIO:**

.. code-block:: ini

   ; platformio.ini
   [env:myboard]
   platform = your_platform
   board = your_board
   framework = arduino  ; or other framework
   lib_deps =
       https://github.com/hathach/tinyusb.git

**CMake FetchContent:**

.. code-block:: cmake

   include(FetchContent)
   FetchContent_Declare(
     tinyusb
     GIT_REPOSITORY https://github.com/hathach/tinyusb.git
     GIT_TAG        master  # or specific version tag
   )
   FetchContent_MakeAvailable(tinyusb)

Method 3: Direct Copy
---------------------

Copy TinyUSB source files directly into your project.

.. code-block:: bash

   # Copy only source files
   cp -r tinyusb/src/ your_project/lib/tinyusb/

**Note:** You'll need to manually update when TinyUSB releases new versions.

Build System Integration
========================

Make/GCC Integration
--------------------

**Makefile example:**

.. code-block:: make

   # TinyUSB settings
   TUSB_DIR = lib/tinyusb
   TUSB_SRC_DIR = $(TUSB_DIR)/src

   # Include paths
   CFLAGS += -I$(TUSB_SRC_DIR)
   CFLAGS += -I.  # For tusb_config.h

   # MCU and OS settings (pass to compiler)
   CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_STM32F4
   CFLAGS += -DCFG_TUSB_OS=OPT_OS_NONE

   # TinyUSB source files
   SRC_C += $(wildcard $(TUSB_SRC_DIR)/*.c)
   SRC_C += $(wildcard $(TUSB_SRC_DIR)/common/*.c)
   SRC_C += $(wildcard $(TUSB_SRC_DIR)/device/*.c)
   SRC_C += $(wildcard $(TUSB_SRC_DIR)/class/*/*.c)
   SRC_C += $(wildcard $(TUSB_SRC_DIR)/portable/$(VENDOR)/$(CHIP_FAMILY)/*.c)

**Finding the right portable driver:**

.. code-block:: bash

   # List available drivers
   find lib/tinyusb/src/portable -name "*.c" | grep stm32
   # Use: lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c

CMake Integration
-----------------

**CMakeLists.txt example:**

.. code-block:: cmake

   # TinyUSB configuration
   set(FAMILY_MCUS STM32F4)  # Set your MCU family
   set(CFG_TUSB_MCU OPT_MCU_STM32F4)
   set(CFG_TUSB_OS OPT_OS_FREERTOS)  # or OPT_OS_NONE

   # Add TinyUSB
   add_subdirectory(lib/tinyusb)

   # Your project
   add_executable(your_app
       src/main.c
       src/usb_descriptors.c
       # other sources
   )

   # Link TinyUSB
   target_link_libraries(your_app
       tinyusb_device  # or tinyusb_host
       # other libraries
   )

   # Include paths
   target_include_directories(your_app PRIVATE
       src/  # For tusb_config.h
   )

   # Compile definitions
   target_compile_definitions(your_app PRIVATE
       CFG_TUSB_MCU=${CFG_TUSB_MCU}
       CFG_TUSB_OS=${CFG_TUSB_OS}
   )

IAR Embedded Workbench
----------------------

Use project connection files for easy integration:

1. Open IAR project
2. Add TinyUSB project connection: ``Tools → Configure Custom Argument Variables``
3. Create ``TUSB`` group, add ``TUSB_DIR`` variable
4. Import ``tinyusb/tools/iar_template.ipcf``

Keil µVision
------------

.. code-block:: none

   # Add to project groups:
   TinyUSB/Common: src/common/*.c
   TinyUSB/Device: src/device/*.c, src/class/*/*.c
   TinyUSB/Portable: src/portable/vendor/family/*.c

   # Include paths:
   src/  # tusb_config.h location
   lib/tinyusb/src/

   # Preprocessor defines:
   CFG_TUSB_MCU=OPT_MCU_STM32F4
   CFG_TUSB_OS=OPT_OS_NONE

Configuration Setup
===================

Create tusb_config.h
--------------------

This is the most critical file for TinyUSB integration:

.. code-block:: c

   // tusb_config.h
   #ifndef _TUSB_CONFIG_H_
   #define _TUSB_CONFIG_H_

   // MCU selection - REQUIRED
   #ifndef CFG_TUSB_MCU
   #define CFG_TUSB_MCU OPT_MCU_STM32F4
   #endif

   // OS selection - REQUIRED
   #ifndef CFG_TUSB_OS
   #define CFG_TUSB_OS OPT_OS_NONE
   #endif

   // Debug level
   #define CFG_TUSB_DEBUG 0

   // Device stack
   #define CFG_TUD_ENABLED 1
   #define CFG_TUD_ENDPOINT0_SIZE 64

   // Device classes
   #define CFG_TUD_CDC 1
   #define CFG_TUD_HID 0
   #define CFG_TUD_MSC 0

   // CDC configuration
   #define CFG_TUD_CDC_EP_BUFSIZE 512
   #define CFG_TUD_CDC_RX_BUFSIZE 512
   #define CFG_TUD_CDC_TX_BUFSIZE 512

   #endif

USB Descriptors
---------------

Create or modify ``usb_descriptors.c`` for your device:

.. code-block:: c

   #include "tusb.h"

   // Device descriptor
   tusb_desc_device_t const desc_device = {
       .bLength            = sizeof(tusb_desc_device_t),
       .bDescriptorType    = TUSB_DESC_DEVICE,
       .bcdUSB             = 0x0200,
       .bDeviceClass       = TUSB_CLASS_MISC,
       .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
       .bDeviceProtocol    = MISC_PROTOCOL_IAD,
       .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
       .idVendor           = 0xCafe,  // Your VID
       .idProduct          = 0x4000,  // Your PID
       .bcdDevice          = 0x0100,
       .iManufacturer      = 0x01,
       .iProduct           = 0x02,
       .iSerialNumber      = 0x03,
       .bNumConfigurations = 0x01
   };

   // Get device descriptor
   uint8_t const* tud_descriptor_device_cb(void) {
       return (uint8_t const*)&desc_device;
   }

   // Configuration descriptor - implement based on your needs
   uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
       // Return configuration descriptor
   }

   // String descriptors
   uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
       // Return string descriptors
   }

Application Integration
======================

Main Loop Integration
--------------------

.. code-block:: c

   #include "tusb.h"

   int main(void) {
       // Board/MCU initialization
       board_init();  // Your board setup

       // USB stack initialization
       tusb_init();

       while (1) {
           // USB device task - MUST be called regularly
           tud_task();

           // Your application code
           your_app_task();
       }
   }

Interrupt Handler Setup
-----------------------

**STM32 example:**

.. code-block:: c

   // USB interrupt handler
   void OTG_FS_IRQHandler(void) {
       tud_int_handler(0);
   }

**RP2040 example:**

.. code-block:: c

   void isr_usbctrl(void) {
       tud_int_handler(0);
   }

Class Implementation
--------------------

Implement required callbacks for enabled classes:

.. code-block:: c

   // CDC class callbacks
   void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding) {
       // Handle line coding changes
   }

   void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
       // Handle DTR/RTS changes
   }

RTOS Integration
===============

FreeRTOS Integration
-------------------

.. code-block:: c

   // USB task
   void usb_device_task(void* param) {
       while (1) {
           tud_task();
           vTaskDelay(1);  // 1ms delay
       }
   }

   // Create USB task
   xTaskCreate(usb_device_task, "usbd",
               256, NULL, configMAX_PRIORITIES-1, NULL);

**Configuration:**

.. code-block:: c

   // In tusb_config.h
   #define CFG_TUSB_OS OPT_OS_FREERTOS
   #define CFG_TUD_TASK_QUEUE_SZ 16

RT-Thread Integration
--------------------

.. code-block:: c

   // In tusb_config.h
   #define CFG_TUSB_OS OPT_OS_RTTHREAD

   // USB thread
   void usb_thread_entry(void* parameter) {
       tusb_init();
       while (1) {
           tud_task();
           rt_thread_mdelay(1);
       }
   }

Custom Hardware Integration
===========================

Clock Configuration
-------------------

USB requires precise 48MHz clock:

**STM32 example:**

.. code-block:: c

   // Configure PLL for 48MHz USB clock
   RCC_OscInitStruct.PLL.PLLQ = 7;  // Adjust for 48MHz
   HAL_RCC_OscConfig(&RCC_OscInitStruct);

**RP2040 example:**

.. code-block:: c

   // USB clock is automatically configured by SDK

Pin Configuration
-----------------

Configure USB pins correctly:

**STM32 example:**

.. code-block:: c

   // USB pins: PA11 (DM), PA12 (DP)
   GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
   GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

Power Management
---------------

For battery-powered applications:

.. code-block:: c

   // Implement suspend/resume callbacks
   void tud_suspend_cb(bool remote_wakeup_en) {
       // Enter low power mode
   }

   void tud_resume_cb(void) {
       // Exit low power mode
   }

Testing and Validation
======================

Build Verification
------------------

.. code-block:: bash

   # Test build
   make clean && make all

   # Check binary size
   arm-none-eabi-size build/firmware.elf

   # Verify no undefined symbols
   arm-none-eabi-nm build/firmware.elf | grep " U "

Runtime Testing
---------------

1. **Device Recognition**: Check if device appears in system
2. **Enumeration**: Verify all descriptors are valid
3. **Class Functionality**: Test class-specific features
4. **Performance**: Measure transfer rates and latency
5. **Stress Testing**: Long-running tests with connect/disconnect

Debugging Integration Issues
============================

Common Problems
---------------

1. **Device not recognized**: Check descriptors and configuration
2. **Build errors**: Verify include paths and source files
3. **Link errors**: Check library dependencies
4. **Runtime crashes**: Enable debug builds and use debugger
5. **Poor performance**: Profile code and optimize critical paths

Debug Builds
------------

.. code-block:: c

   // In tusb_config.h for debugging
   #define CFG_TUSB_DEBUG 2
   #define CFG_TUSB_DEBUG_PRINTF printf

Enable logging to identify issues quickly.

Production Considerations
=========================

Code Size Optimization
----------------------

.. code-block:: c

   // Minimal configuration
   #define CFG_TUSB_DEBUG 0
   #define CFG_TUD_CDC 1
   #define CFG_TUD_HID 0
   #define CFG_TUD_MSC 0
   // Disable unused classes

Performance Optimization
------------------------

- Use DMA for USB transfers if available
- Optimize descriptor sizes
- Use appropriate endpoint buffer sizes
- Consider high-speed USB for high bandwidth applications

Compliance and Certification
----------------------------

- Validate descriptors against USB specifications
- Test with USB-IF compliance tools
- Consider USB-IF certification for commercial products
- Test with multiple host operating systems