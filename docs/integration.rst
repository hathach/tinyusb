*******************
Integrating TinyUSB
*******************

Once you've seen TinyUSB working in the examples, use this guide to wire the stack into your own firmware.

Integration Steps
=================

1. **Get TinyUSB**: Copy this repository or add it as a git submodule to your project at ``your_project/tinyusb``.
2. **Add source files**: Add every ``.c`` file from ``tinyusb/src/`` to your project build system.

.. note::
   Only supported dcd/hcd drivers for your CPU sources under ``tinyusb/src/portable/vendor/usbip/`` are needed. Add

3. **Configure TinyUSB**: Create ``tusb_config.h`` with macros such as ``CFG_TUSB_MCU``, ``CFG_TUSB_OS``, and class enable flags. Start from any example's ``tusb_config.h`` and tweak.
4. **Configure include paths**: Add ``your_project/tinyusb/src`` (and the folder holding ``tusb_config.h``) to your include paths.
5. **Implement USB descriptors**: For device stack, implement the ``tud_descriptor_*_cb()`` callbacks (device) or host descriptor helpers that match your product.
6. **Initialize TinyUSB**: Call ``tusb_init()`` once the clocks/peripherals are ready. Pass ``tusb_rhport_init_t`` if you need per-port settings.
7. **Handle interrupts**: From the USB ISR call ``tusb_int_handler(rhport, true)`` so the stack can process events.
8. **Run USB tasks**: Call ``tud_task()`` (device) or ``tuh_task()`` (host) regularly from the main loop, RTOS task.
9. **Implement class callbacks**: Provide the callbacks for the classes you enabled (e.g., ``tud_cdc_rx_cb()``, ``tuh_msc_mount_cb()``).

Minimal Example
===============

.. code-block:: c

   #include "tusb.h"

   int main(void) {
     board_init();  // Your board initialization

     // Init device stack on roothub port 0 for highspeed device
     tusb_rhport_init_t dev_init = {
       .role  = TUSB_ROLE_DEVICE,
       .speed = TUSB_SPEED_HIGH
     };
     tusb_init(0, &dev_init);

     // init host stack on roothub port 1 for fullspeed host
     tusb_rhport_init_t host_init = {
       .role  = TUSB_ROLE_DEVICE,
       .speed = TUSB_SPEED_FULL
     };
     tusb_init(1, &host_init);

     while (1) {
       tud_task();  // device task
       tuh_task();  // host task

       app_task();  // Your application logic
     }
   }

   void USB0_IRQHandler(void) {
     // forward interrupt port 0 to TinyUSB stack
     tusb_int_handler(0, true);
   }

   void USB1_IRQHandler(void) {
     // forward interrupt port 0 to TinyUSB stack
     tusb_int_handler(1, true);
   }

.. note::
   Unlike many libraries, TinyUSB callbacks don't need to be registered. Implement functions with the prescribed names (for example ``tud_cdc_rx_cb()``) and the stack will invoke them automatically.

.. note::
   Naming follows ``tud_*`` for device APIs and ``tuh_*`` for host APIs. Refer to :doc:`reference/glossary` for a summary of the prefixes and callback naming rules.


STM32CubeIDE Integration
========================

To integrate TinyUSB device stack with STM32CubeIDE

1. In STM32CubeMX, enable USB_OTG_FS/HS under Connectivity, set to "Device_Only" mode
2. Enable the USB global interrupt in NVIC Settings
3. Add ``tusb.h`` include and call ``tusb_init()`` in main.c
4. Call ``tud_task()`` in your main loop
5. In the generated ``stm32xxx_it.c``, modify the USB IRQ handler to call ``tud_int_handler(0)``

.. code-block:: c

   void OTG_FS_IRQHandler(void) {
     tud_int_handler(0);
   }

6. Create ``tusb_config.h`` and ``usb_descriptors.c`` files

.. tip::
   STM32CubeIDE generated code conflicts with TinyUSB. Don't use STM32's built-in USB middleware (USB Device Library) when using TinyUSB. Disable USB code generation in STM32CubeMX and let TinyUSB handle all USB functionality.
