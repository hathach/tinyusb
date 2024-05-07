*****************
Supported Devices
*****************

Supported MCUs
==============

+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Manufacturer | Family                | Device | Host | Highspeed | Driver            | Note         |
+==============+=======================+========+======+===========+===================+==============+
| Allwinner    | F1C100s/F1C200s       | ✔      |      | ✔         | sunxi             | musb variant |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Analog       | MAX3421E              |        | ✔    | ✖         | max3421           | via SPI      |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Brigetek     | FT90x                 | ✔      |      | ✔         | ft9xx             |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Broadcom     | BCM2711, BCM2837      | ✔      |      | ✔         | dwc2              |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Dialog       | DA1469x               | ✔      | ✖    | ✖         | da146xx           |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Espressif    | ESP32 S2, S3          | ✔      |      | ✖         | dwc2 or esp32sx   |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| GigaDevice   | GD32VF103             | ✔      |      | ✖         | dwc2              |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Infineon     | XMC4500               | ✔      |      | ✖         | dwc2              |              |
+--------------+-----+-----------------+--------+------+-----------+-------------------+--------------+
| MicroChip    | SAM | D11, D21        | ✔      |      | ✖         | samd              |              |
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | D51, E5x        | ✔      |      | ✖         | samd              |              |
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | G55             | ✔      |      | ✖         | samg              |              |
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | L21, L22        | ✔      |      | ✖         | samd              |              |
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | E70,S70,V70,V71 | ✔      |      | ✔         | samx7x            |              |
|              +-----+-----------------+--------+------+-----------+-------------------+--------------+
|              | PIC | 24              | ✔      |      |           | pic               | ci_fs variant|
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | 32 mm, mk, mx   | ✔      |      |           | pic               | ci_fs variant|
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | dsPIC33         | ✔      |      |           | pic               | ci_fs variant|
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | 32mz            | ✔      |      |           | pic32mz           | musb variant |
+--------------+-----+-----------------+--------+------+-----------+-------------------+--------------+
| Mind Montion | mm32                  | ✔      |      | ✖         | mm32f327x_otg     | ci_fs variant|
+--------------+-----+-----------------+--------+------+-----------+-------------------+--------------+
| NordicSemi   | nRF52833, nRF52840    | ✔      | ✖    | ✖         | nrf5x             |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | nRF5340               | ✔      | ✖    | ✖         | nrf5x             |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Nuvoton      | NUC120                | ✔      | ✖    | ✖         | nuc120            |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | NUC121/NUC125         | ✔      | ✖    | ✖         | nuc121            |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | NUC126                | ✔      | ✖    | ✖         | nuc121            |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | NUC505                | ✔      |      | ✔         | nuc505            |              |
+--------------+---------+-------------+--------+------+-----------+-------------------+--------------+
| NXP          | iMXRT   | RT10xx      | ✔      | ✔    | ✔         | ci_hs             |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | RT11xx      | ✔      | ✔    | ✔         | ci_hs             |              |
|              +---------+-------------+--------+------+-----------+-------------------+--------------+
|              | Kinetis | KL          | ✔      | ⚠    | ✖         | ci_fs, khci       |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | K32L2       | ✔      |      | ✖         | khci              | ci_fs variant|
|              +---------+-------------+--------+------+-----------+-------------------+--------------+
|              | LPC     | 11u, 13, 15 | ✔      | ✖    | ✖         | lpc_ip3511        |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | 17, 40      | ✔      | ⚠    | ✖         | lpc17_40          |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | 18, 43      | ✔      | ✔    | ✔         | ci_hs             |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | 51u         | ✔      | ✖    | ✖         | lpc_ip3511        |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | 54          | ✔      |      | ✔         | lpc_ip3511        |              |
|              |         +-------------+--------+------+-----------+-------------------+--------------+
|              |         | 55          | ✔      |      | ✔         | lpc_ip3511        |              |
|              +---------+-------------+--------+------+-----------+-------------------+--------------+
|              | MCX     | N9          | ✔      |      | ✔         | ci_fs, ci_hs      |              |
+--------------+---------+-------------+--------+------+-----------+-------------------+--------------+
| Raspberry Pi | RP2040                | ✔      | ✔    | ✖         | rp2040, pio_usb   |              |
+--------------+-----+-----------------+--------+------+-----------+-------------------+--------------+
| Renesas      | RX  | 63N, 65N, 72N   | ✔      | ✔    | ✖         | rusb2             |              |
|              +-----+-----------------+--------+------+-----------+-------------------+--------------+
|              | RA  | 4M1, 4M3, 6M1   | ✔      | ✔    | ✖         | rusb2             |              |
|              |     +-----------------+--------+------+-----------+-------------------+--------------+
|              |     | 6M5             | ✔      | ✔    | ✔         | rusb2             |              |
+--------------+-----+-----------------+--------+------+-----------+-------------------+--------------+
| Silabs       | EFM32GG12             | ✔      |      | ✖         | dwc2              |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| Sony         | CXD56                 | ✔      | ✖    | ✔         | cxd56             |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| ST STM32     | F0                    | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              +----+------------------+--------+------+-----------+-------------------+--------------+
|              | F1 | 102, 103         | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              |    +------------------+--------+------+-----------+-------------------+--------------+
|              |    | 105, 107         | ✔      |      | ✖         | dwc2              |              |
|              +----+------------------+--------+------+-----------+-------------------+--------------+
|              | F2                    | ✔      |      | ✔         | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | F3                    | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | F4                    | ✔      |      | ✔         | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | F7                    | ✔      |      | ✔         | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | H7                    | ✔      |      | ✔         | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | G4                    | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | L0, L1                | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              +----+------------------+--------+------+-----------+-------------------+--------------+
|              | L4 | 4x2, 4x3         | ✔      | ✖    | ✖         | stm32_fsdev       |              |
|              |    +------------------+--------+------+-----------+-------------------+--------------+
|              |    | 4x5, 4x6         | ✔      |      |           | dwc2              |              |
|              +----+------------------+--------+------+-----------+-------------------+--------------+
|              | L4+                   | ✔      |      |           | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | U5                    | ✔      |      | ✔         | dwc2              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | WBx5                  | ✔      |      |           | stm32_fsdev       |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| TI           | MSP430                | ✔      | ✖    | ✖         | msp430x5xx        |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | MSP432E4              | ✔      |      | ✖         | musb              |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | TM4C123               | ✔      |      | ✖         | musb              |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| ValentyUSB   | eptri                 | ✔      | ✖    | ✖         | eptri             |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+
| WCH          | CH32V307              | ✔      |      | ✔         | ch32v307          |              |
|              +-----------------------+--------+------+-----------+-------------------+--------------+
|              | CH32F20x              | ✔      |      | ✔         | ch32f205          |              |
+--------------+-----------------------+--------+------+-----------+-------------------+--------------+


Table Legend
------------

= ===================
✔ Supported
⚠ WIP/partial support
✖ Not supported
= ===================

Supported Boards
================

The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project. It is responsible for getting the MCU started and the USB peripheral clocked with minimal of on-board devices

-  One LED : for status
-  One Button : to get input from user
-  One UART : optional for device, but required for host examples

The following boards are supported (sorted alphabetically):

Broadcom
--------

-  `Raspberry Pi CM4 <https://www.raspberrypi.com/products/compute-module-4>`__

Dialog DA146xx
--------------

-  `DA14695 Development Kit – USB <https://www.dialog-semiconductor.com/products/da14695-development-kit-usb>`__
-  `DA1469x Development Kit – Pro <https://www.dialog-semiconductor.com/products/da14695-development-kit-pro>`__

Espressif ESP32-S2
------------------

-  `Adafruit Feather ESP32-S2 <https://www.adafruit.com/product/5000>`__
-  `Adafruit Magtag 2.9" E-Ink WiFi Display <https://www.adafruit.com/product/4800>`__
-  `Adafruit Metro ESP32-S2 <https://www.adafruit.com/product/4775>`__
-  `ESP32-S2-Kaluga-1 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html>`__
-  `ESP32-S2-Saola-1 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html>`__

GigaDevice
----------

-  `Sipeed Longan Nano <https://longan.sipeed.com/en/>`__

Infineon
---------

XMC4000
^^^^^^^

-  `XMC4500 Relax (Lite) Kit <https://www.infineon.com/cms/en/product/evaluation-boards/kit_xmc45_relax_lite_v1/>`__

MicroChip
---------

SAMD11 & SAMD21
^^^^^^^^^^^^^^^

-  `Adafruit Circuit Playground Express <https://www.adafruit.com/product/3333>`__
-  `Adafruit Feather M0 Express <https://www.adafruit.com/product/3403>`__
-  `Adafruit ItsyBitsy M0 Express <https://www.adafruit.com/product/3727>`__
-  `Adafruit Metro M0 Express <https://www.adafruit.com/product/3505>`__
-  `Great Scott Gadgets Cynthion <https://greatscottgadgets.com/cynthion/>`__
-  `Microchip SAMD11 Xplained Pro <https://www.microchip.com/developmenttools/ProductDetails/atsamd11-xpro>`__
-  `Microchip SAMD21 Xplained Pro <https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAMD21-XPRO>`__
-  `Seeeduino Xiao <https://www.seeedstudio.com/Seeeduino-XIAO-Arduino-Microcontroller-SAMD21-Cortex-M0+-p-4426.html>`__

SAMD51 & SAME54
^^^^^^^^^^^^^^^

-  `Adafruit Feather M4 Express <https://www.adafruit.com/product/3857>`__
-  `Adafruit ItsyBitsy M4 Express <https://www.adafruit.com/product/3800>`__
-  `Adafruit PyBadge <https://www.adafruit.com/product/4200>`__
-  `Adafruit PyPortal <https://www.adafruit.com/product/4116>`__
-  `Adafruit Metro M4 Express <https://www.adafruit.com/product/3382>`__
-  `D5035-01 <https://github.com/RudolphRiedel/USB_CAN-FD>`__
-  `Microchip SAME54 Xplained Pro <https://www.microchip.com/developmenttools/productdetails/atsame54-xpro>`__

SAME7x
^^^^^^

- `Microchip SAME70 Xplained <https://www.microchip.com/en-us/development-tool/ATSAME70-XPLD>`_
- `QMTECH ATSAME70N19 <https://www.aliexpress.com/item/1005003173783268.html>`_

SAMG
^^^^

-  `Microchip SAMG55 Xplained Pro <https://www.microchip.com/DevelopmentTools/ProductDetails/PartNO/ATSAMG55-XPRO>`__

SAML2x
^^^^^^

-  `SAML21 Xplaind Pro <https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAML21-XPRO-B>`__
-  `SAML22 Feather <https://github.com/joeycastillo/Feather-Projects/tree/main/SAML22%20Feather>`__
-  `Sensor Watch <https://github.com/joeycastillo/Sensor-Watch>`__

Nordic nRF5x
------------

-  `Adafruit Circuit Playground Bluefruit <https://www.adafruit.com/product/4333>`__
-  `Adafruit CLUE <https://www.adafruit.com/product/4500>`__
-  `Adafruit Feather nRF52840 Express <https://www.adafruit.com/product/4062>`__
-  `Adafruit Feather nRF52840 Sense <https://www.adafruit.com/product/4516>`__
-  `Adafruit ItsyBitsy nRF52840 Express <https://www.adafruit.com/product/4481>`__
-  `Arduino Nano 33 BLE <https://store.arduino.cc/usa/nano-33-ble>`__
-  `Arduino Nano 33 BLE Sense <https://store.arduino.cc/usa/nano-33-ble-sense>`__
-  `Maker Diary nRF52840 MDK Dongle <https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle>`__
-  `Nordic nRF52840 Development Kit (aka pca10056) <https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK>`__
-  `Nordic nRF52840 Dongle (aka pca10059) <https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-Dongle>`__
-  `Nordic nRF52833 Development Kit (aka pca10100) <https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52833-DK>`__
-  `Raytac MDBT50Q-RX Dongle <https://www.raytac.com/product/ins.php?index_id=89>`__

Nuvoton
-------

-  NuTiny SDK NUC120
-  `NuTiny NUC121S <https://direct.nuvoton.com/en/nutiny-nuc121s>`__
-  `NuTiny NUC125S <https://direct.nuvoton.com/en/nutiny-nuc125s>`__
-  `NuTiny NUC126V <https://direct.nuvoton.com/en/nutiny-nuc126v>`__
-  `NuTiny SDK NUC505Y <https://direct.nuvoton.com/en/nutiny-nuc505y>`__

NXP
---

iMX RT
^^^^^^

-  `MIMX RT1010 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1010-evaluation-kit:MIMXRT1010-EVK>`__
-  `MIMX RT1015 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1015-evaluation-kit:MIMXRT1015-EVK>`__
-  `MIMX RT1020 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1020-evaluation-kit:MIMXRT1020-EVK>`__
-  `MIMX RT1050 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1050-evaluation-kit:MIMXRT1050-EVK>`__
-  `MIMX RT1060 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/mimxrt1060-evk-i.mx-rt1060-evaluation-kit:MIMXRT1060-EVK>`__
-  `MIMX RT1064 Evaluation Kit <https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/mimxrt1064-evk-i.mx-rt1064-evaluation-kit:MIMXRT1064-EVK>`__
-  `Teensy 4.0 Development Board <https://www.pjrc.com/store/teensy40.html>`__
-  `Teensy 4.1 Development Board <https://www.pjrc.com/store/teensy41.html>`__

Kinetis
^^^^^^^

-  `Freedom FRDM-KL25Z <https://www.nxp.com/design/development-boards/freedom-development-boards/mcu-boards/freedom-development-platform-for-kinetis-kl14-kl15-kl24-kl25-mcus:FRDM-KL25Z>`__
-  `Freedom FRDM-K32L2A4S  <https://www.nxp.com/design/development-boards/freedom-development-boards/mcu-boards/nxp-freedom-platform-for-k32-l2a-mcus:FRDM-K32L2A4S>`__
-  `Freedom FRDM-K32L2B3 <https://www.nxp.com/design/development-boards/freedom-development-boards/mcu-boards/nxp-freedom-development-platform-for-k32-l2b-mcus:FRDM-K32L2B3>`__
-  `KUIIC <https://github.com/nxf58843/kuiic>`__

LPC 11-13-15
^^^^^^^^^^^^

-  `LPCXpresso 11u37 <https://www.nxp.com/design/microcontrollers-developer-resources/lpcxpresso-boards/lpcxpresso-board-for-lpc11u37h:OM13074>`__
-  `LPCXpresso 11u68 <https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc11u68:OM13058>`__
-  `LPCXpresso 1347 <https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1347:OM13045>`__
-  `LPCXpresso 1549 <https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc1500-cortex-m3/lpcxpresso-board-for-lpc1549:OM13056>`__

LPC 17-40
^^^^^^^^^

-  `ARM mbed LPC1768 <https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc1700-cortex-m3/arm-mbed-lpc1768-board:OM11043>`__
-  `Embedded Artists LPC4088 Quick Start board <https://www.embeddedartists.com/products/lpc4088-quickstart-board>`__
-  `LPCXpresso 1769 <https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1769:OM13000>`__

LPC 18-43
^^^^^^^^^

-  `Embedded Artists LPC4357 Developer Kit <http://www.embeddedartists.com/products/kits/lpc4357_kit.php>`__
-  `Keil MCB1800 Evaluation Board <http://www.keil.com/mcb1800>`__
-  `LPCXpresso18S37 Development Board <https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc4000-cortex-m4/lpcxpresso18s37-development-board:OM13076>`__

LPC 51
^^^^^^

-  `LPCXpresso 51U68 <https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpcxpresso51u68-for-the-lpc51u68-mcus:OM40005>`__

LPC 54
^^^^^^

-  `LPCXpresso 54114 <https://www.nxp.com/design/microcontrollers-developer-resources/lpcxpresso-boards/lpcxpresso54114-board:OM13089>`__

LPC55
^^^^^

-  `Double M33 Express <https://www.crowdsupply.com/steiert-solutions/double-m33-express>`__
-  `LPCXpresso 55s28 EVK <https://www.nxp.com/design/software/development-software/lpcxpresso55s28-development-board:LPC55S28-EVK>`__
-  `LPCXpresso 55s69 EVK <https://www.nxp.com/design/development-boards/lpcxpresso-boards/lpcxpresso55s69-development-board:LPC55S69-EVK>`__
-  `MCU-Link <https://www.nxp.com/design/development-boards/lpcxpresso-boards/mcu-link-debug-probe:MCU-LINK>`__

Renesas
-------

RA
^^

-  `Evaluation Kit for RA4M1 <https://www.renesas.com/us/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra4m1-evaluation-kit-ra4m1-mcu-group>`__
-  `Evaluation Kit for RA4M3 <https://www.renesas.com/us/en/products/microcontrollers-microprocessors/ra-cortex-m-mcus/ek-ra4m3-evaluation-kit-ra4m3-mcu-group>`__

RX
^^

-  `GR-CITRUS <https://www.renesas.com/us/en/products/gadget-renesas/boards/gr-citrus>`__
-  `Renesas RX65N Target Board <https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rx-32-bit-performance-efficiency-mcus/rtk5rx65n0c00000br-target-board-rx65n>`__

Raspberry Pi RP2040
-------------------

-  `Adafruit Feather RP2040 <https://www.adafruit.com/product/4884>`__
-  `Adafruit ItsyBitsy RP2040 <https://www.adafruit.com/product/4888>`__
-  `Adafruit QT Py RP2040 <https://www.adafruit.com/product/4900>`__
-  `Raspberry Pi Pico <https://www.raspberrypi.org/products/raspberry-pi-pico/>`__

Silabs
------

-  `EFM32GG12 Thunderboard Kit (SLTB009A) <https://www.silabs.com/development-tools/thunderboard/thunderboard-gg12-kit>`__

Sony
----

-  `Sony Spresense CXD5602 <https://developer.sony.com/develop/spresense>`__

ST STM32
--------

F0
^^
-  `STM32 F070rb Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f070rb.html>`__
-  `STM32 F072 Evaluation <https://www.st.com/en/evaluation-tools/stm32072b-eval.html>`__
-  `STM32 F072rb Discovery <https://www.st.com/en/evaluation-tools/32f072bdiscovery.html>`__

F1
^^
-  `STM32 F103c8 Blue Pill <https://stm32-base.org/boards/STM32F103C8T6-Blue-Pill>`__
-  `STM32 F103rc Mini v2.0 <https://stm32-base.org/boards/STM32F103RCT6-STM32-Mini-V2.0>`__

F2
^^
-  `STM32 F207zg Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f207zg.html>`__

F3
^^
-  `STM32 F303vc Discovery <https://www.st.com/en/evaluation-tools/stm32f3discovery.html>`__

F4
^^
-  `Adafruit Feather STM32F405 <https://www.adafruit.com/product/4382>`__
-  `Micro Python PyBoard v1.1 <https://store.micropython.org/product/PYBv1.1>`__
-  `STM32 F401cc Black Pill <https://stm32-base.org/boards/STM32F401CCU6-WeAct-Black-Pill-V1.2>`__
-  `STM32 F407vg Discovery <https://www.st.com/en/evaluation-tools/stm32f4discovery.html>`__
-  `STM32 F411ce Black Pill <https://www.adafruit.com/product/4877>`__
-  `STM32 F411ve Discovery <https://www.st.com/en/evaluation-tools/32f411ediscovery.html>`__
-  `STM32 F412zg Discovery <https://www.st.com/en/evaluation-tools/32f412gdiscovery.html>`__
-  `STM32 F412zg Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f412zg.html>`__
-  `STM32 F439zi Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f439zi.html>`__

F7
^^

-  `STLink-V3 Mini <https://www.st.com/en/development-tools/stlink-v3mini.html>`__
-  `STM32 F723e Discovery <https://www.st.com/en/evaluation-tools/32f723ediscovery.html>`__
-  `STM32 F746zg Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f746zg.html>`__
-  `STM32 F746g Discovery <https://www.st.com/en/evaluation-tools/32f746gdiscovery.html>`__
-  `STM32 F767zi Nucleo <https://www.st.com/en/evaluation-tools/nucleo-f767zi.html>`__
-  `STM32 F769i Discovery <https://www.st.com/en/evaluation-tools/32f769idiscovery.html>`__

H7
^^
-  `STM32 H743zi Nucleo <https://www.st.com/en/evaluation-tools/nucleo-h743zi.html>`__
-  `STM32 H743i Evaluation <https://www.st.com/en/evaluation-tools/stm32h743i-eval.html>`__
-  `STM32 H745i Discovery <https://www.st.com/en/evaluation-tools/stm32h745i-disco.html>`__
-  `Waveshare OpenH743I-C <https://www.waveshare.com/openh743i-c-standard.htm>`__

G4
^^
-  `STM32 G474RE Nucleo <https://www.st.com/en/evaluation-tools/nucleo-g474re.html>`__

L0
^^
-  `STM32 L035c8 Discovery <https://www.st.com/en/evaluation-tools/32l0538discovery.html>`__

L4
^^
-  `STM32 L476vg Discovery <https://www.st.com/en/evaluation-tools/32l476gdiscovery.html>`__
-  `STM32 L4P5zg Nucleo <https://www.st.com/en/evaluation-tools/nucleo-l4p5zg.html>`__
-  `STM32 L4R5zi Nucleo <https://www.st.com/en/evaluation-tools/nucleo-l4r5zi.html>`__

WB
^^
-  `STM32 WB55 Nucleo <https://www.st.com/en/evaluation-tools/p-nucleo-wb55.html>`__

TI
--

-  `MSP430F5529 USB LaunchPad Evaluation Kit <http://www.ti.com/tool/MSP-EXP430F5529LP>`__
-  `MSP-EXP432E401Y LaunchPad Evaluation Kit <https://www.ti.com/tool/MSP-EXP432E401Y>`__
-  `TM4C123GXL LaunchPad Evaluation Kit <https://www.ti.com/tool/EK-TM4C123GXL>`__

Tomu
----

-  `Fomu <https://www.crowdsupply.com/sutajio-kosagi/fomu>`__

WCH
---

-  `CH32V307V-R1-1v0 <https://lcsc.com/product-detail/Development-Boards-Kits_WCH-Jiangsu-Qin-Heng-CH32V307V-EVT-R1_C2943980.html>`__
-  `CH32F205R-R0-1v0 <https://github.com/openwch/ch32f20x/blob/main/EVT/PUB/CH32F20x%20Evaluation%20Board%20Reference-EN.pdf>`__
