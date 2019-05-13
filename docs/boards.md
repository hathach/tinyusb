# Boards

The board support code is only used for self-contained examples and testing. It is not used when TinyUSB is part of a larger project. It is responsible for getting the MCU started and the USB peripheral clocked with minimal of on-board devices
- One LED for status
- One Button to get input from user
- One UART optionally, mostly for host examples
 
## Supported Boards

This code base already had supported for a handful of following boards

### Nordic nRF5x

- [Adafruit Feather nRF52840 Express](https://www.adafruit.com/product/4062)
- [Nordic nRF52840 Development Kit (aka pca10056)](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK)

### NXP LPC

- [LPCXpresso 11U68](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc11u68:OM13058)
- [LPCXpresso 1347](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1347:OM13045)
- [LPCXpresso 1769](https://www.nxp.com/support/developer-resources/evaluation-and-development-boards/lpcxpresso-boards/lpcxpresso-board-for-lpc1769:OM13000)
- [Keil MCB1800 Evaluation Board](http://www.keil.com/mcb1800)
- [Embedded Artists LPC4088 Quick Start board](https://www.embeddedartists.com/products/lpc4088-quickstart-board)
- [Embedded Artists LPC4357 Developer Kit](http://www.embeddedartists.com/products/kits/lpc4357_kit.php)

### MicroChip SAMD

- [Adafruit Metro M0 Express](https://www.adafruit.com/product/3505)
- [Adafruit Metro M4 Express](https://www.adafruit.com/product/3382)

### ST STM32

- [STM32F4 Discovery](https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

## Add your own board

If you don't possess any of supported board above. Don't worry you can easily implemented your own one by following this guide as long as the mcu is supported.

- Create new makefile for your board at `hw/bsp/<board name>/board.mk` and linker file as well if needed.
- Create new source file for your board at `hw/bsp/<board name>/board_<board name>.c` and implement following APIs

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
