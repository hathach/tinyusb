# Boards

The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project. It is responsible for getting the MCU started and the USB peripheral clocked with minimal of on-board devices
- One LED : for status
- One Button : to get input from user
- One UART : optional for device, but required for host examples
 
## Supported Boards

This code base already had supported for a handful of following boards (sorted alphabetically)

### Espressif ESP32-S2

- [ESP32-S2-Saola-1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html)

### MicroChip SAMD

- [Adafruit Circuit Playground Express](https://www.adafruit.com/product/3333)
- [Adafruit Feather M0 Express](https://www.adafruit.com/product/3403)
- [Adafruit Feather M4 Express](https://www.adafruit.com/product/3857)
- [Adafruit ItsyBitsy M0 Express](https://www.adafruit.com/product/3727)
- [Adafruit ItsyBitsy M4 Express](https://www.adafruit.com/product/3800)
- [Adafruit Metro M0 Express](https://www.adafruit.com/product/3505)
- [Adafruit Metro M4 Express](https://www.adafruit.com/product/3382)
- [Seeeduino Xiao](https://www.seeedstudio.com/Seeeduino-XIAO-Arduino-Microcontroller-SAMD21-Cortex-M0+-p-4426.html)

### Nordic nRF5x

- [Adafruit Circuit Playground Bluefruit](https://www.adafruit.com/product/4333)
- [Adafruit CLUE](https://www.adafruit.com/product/4500)
- [Adafruit Feather nRF52840 Express](https://www.adafruit.com/product/4062)
- [Adafruit Feather nRF52840 Sense](https://www.adafruit.com/product/4516)
- [Arduino Nano 33 BLE](https://store.arduino.cc/usa/nano-33-ble)
- [Arduino Nano 33 BLE Sense](https://store.arduino.cc/usa/nano-33-ble-sense)
- [Maker Diary nRF52840 MDK Dongle](https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle)
- [Nordic nRF52840 Development Kit (aka pca10056)](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK)
- [Nordic nRF52840 Dongle (aka pca10059)](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-Dongle)
- [Nordic nRF52833 Development Kit (aka pca10100)](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52833-DK)
- [Raytac MDBT50Q-RX Dongle](https://www.raytac.com/product/ins.php?index_id=89)

### Nuvoton

- NuTiny SDK NUC120
- [NuTiny NUC121S](https://direct.nuvoton.com/en/nutiny-nuc121s)
- [NuTiny NUC125S](https://direct.nuvoton.com/en/nutiny-nuc125s)
- [NuTiny NUC126V](https://direct.nuvoton.com/en/nutiny-nuc126v)
- [NuTiny SDK NUC505Y](https://direct.nuvoton.com/en/nutiny-nuc505y)

### NXP iMX RT

- [MIMX RT1010 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1010-evaluation-kit:MIMXRT1010-EVK)
- [MIMX RT1015 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1015-evaluation-kit:MIMXRT1015-EVK)
- [MIMX RT1020 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1020-evaluation-kit:MIMXRT1020-EVK)
- [MIMX RT1050 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/i.mx-rt1050-evaluation-kit:MIMXRT1050-EVK)
- [MIMX RT1060 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/mimxrt1060-evk-i.mx-rt1060-evaluation-kit:MIMXRT1060-EVK)
- [MIMX RT1064 Evaluation Kit](https://www.nxp.com/design/development-boards/i.mx-evaluation-and-development-boards/mimxrt1064-evk-i.mx-rt1064-evaluation-kit:MIMXRT1064-EVK)
- [Teensy 4.0 Development Board](https://www.pjrc.com/store/teensy40.html)

### NXP LPC

- [ARM mbed LPC1768](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc1700-cortex-m3/arm-mbed-lpc1768-board:OM11043)
- [Embedded Artists LPC4088 Quick Start board](https://www.embeddedartists.com/products/lpc4088-quickstart-board)
- [Embedded Artists LPC4357 Developer Kit](http://www.embeddedartists.com/products/kits/lpc4357_kit.php)
- [Keil MCB1800 Evaluation Board](http://www.keil.com/mcb1800)
- [LPCXpresso 11u37](https://www.nxp.com/design/microcontrollers-developer-resources/lpcxpresso-boards/lpcxpresso-board-for-lpc11u37h:OM13074)
- [LPCXpresso 11u68](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc11u68:OM13058)
- [LPCXpresso 1347](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1347:OM13045)
- [LPCXpresso 1769](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1769:OM13000)
- [LPCXpresso 51U68](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpcxpresso51u68-for-the-lpc51u68-mcus:OM40005)
- [LPCXpresso 54114](https://www.nxp.com/design/microcontrollers-developer-resources/lpcxpresso-boards/lpcxpresso54114-board:OM13089)
- [LPCXpresso 55s69 EVK](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/general-purpose-mcus/lpc5500-cortex-m33/lpcxpresso55s69-development-board:LPC55S69-EVK)
- [NGX LPC4330-Xplorer](https://www.nxp.com/design/designs/lpc4330-xplorer-board:OM13027)

### Sony

- [Sony Spresense CXD5602](https://developer.sony.com/develop/spresense)

### ST STM32

- [Adafruit Feather STM32F405](https://www.adafruit.com/product/4382)
- [Micro Python PyBoard v1.1](https://store.micropython.org/product/PYBv1.1)
- [STM32 L035c8 Discovery](https://www.st.com/en/evaluation-tools/32l0538discovery.html)
- [STM32 F070rb Nucleo](https://www.st.com/en/evaluation-tools/nucleo-f070rb.html)
- [STM32 F072rb Discovery](https://www.st.com/en/evaluation-tools/32f072bdiscovery.html)
- STM32 F103c Blue Pill
- [STM32 F207zg Nucleo](https://www.st.com/en/evaluation-tools/nucleo-f207zg.html)
- [STM32 F303vc Discovery](https://www.st.com/en/evaluation-tools/stm32f3discovery.html)
- STM32 F401cc Black Pill
- [STM32 F407vg Discovery](https://www.st.com/en/evaluation-tools/stm32f4discovery.html)
- STM32 F411ce Black Pill
- [STM32 F411ve Discovery](https://www.st.com/en/evaluation-tools/32f411ediscovery.html)
- [STM32 F412zg Discovery](https://www.st.com/en/evaluation-tools/32f412gdiscovery.html)
- [STM32 F767zi Nucleo](https://www.st.com/en/evaluation-tools/nucleo-f767zi.html)
- [STM32 H743zi Nucleo](https://www.st.com/en/evaluation-tools/nucleo-h743zi.html)

### TI

 - [MSP430F5529 USB LaunchPad Evaluation Kit](http://www.ti.com/tool/MSP-EXP430F5529LP)
 
### Tomu

- [Fomu](https://www.crowdsupply.com/sutajio-kosagi/fomu)

## Add your own board

If you don't possess any of supported board above. Don't worry you can easily implemented your own one by following this guide as long as the mcu is supported.

- Create new makefile for your board at `hw/bsp/<board name>/board.mk` and linker file as well if needed.
- Create new source file for your board at `hw/bsp/<board name>/<board name>.c` and implement following APIs

### Board APIs

#### board_init()

Is responsible for starting the MCU, setting up the USB clock and USB pins. It is also responsible for initializing LED and button pins.
One useful clock debugging technique is to set up a PWM output at a known value such as 500hz based on the USB clock so that you can verify it is correct with a logic probe or oscilloscope.
Setup your USB in a crystal-less mode when available. That makes the code easier to port across boards.

#### board_led_write()

Set the pin corresponding to the led to output a value that lights the LED when `state` is true.

#### board_button_read()

Return current state of button, a `1` means active (pressed), a `0` means inactive.

#### board_millis()

The function returns the elapsed number of milliseconds since startup. On ARM this is commonly done with SysTick or Timer. This provide examples a way to measure time to blink LED or delay properly. It is only required when run examples without RTOS `CFG_TUSB_OS == OPT_OS_NONE`.

#### board_uart_read()

Get characters from UART peripheral.

####  board_uart_write()

Send characters to UART peripheral.
