# BSP support for F1Cx00s boards

This folder contains necessary file and scripts to run TinyUSB examples on F1Cx00s boards.

Currently tested on:

* Lichee Pi Nano (F1C100s)

## make flash
`make flash` will use [xfel](https://github.com/xboot/xfel) to write the code to onchip DDR memory and execute it. It will not write the program to SPI Flash.

To enter FEL mode, you have to press BOOT button, then press RESET once, and release BOOT button. You will find VID/PID=1f3a:efe8 on your PC.

## TODO
* Test on Tiny200 v2 (F1C200s)
* Make it able to load .bin directly to SPI Flash and boot
* Add F1C100s to `#if CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX` high speed MCU check in examples (maybe we should extract the logic?)
* `cdc_msc` example is not working. Device is only echoing the first character.