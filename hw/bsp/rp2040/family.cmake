# TinyUSB Stack source
set(SRC_TINYUSB
	${TOP}/src/tusb.c
	${TOP}/src/common/tusb_fifo.c
	${TOP}/src/device/usbd.c
	${TOP}/src/device/usbd_control.c
	${TOP}/src/class/audio/audio_device.c
	${TOP}/src/class/cdc/cdc_device.c
	${TOP}/src/class/dfu/dfu_rt_device.c
	${TOP}/src/class/hid/hid_device.c
	${TOP}/src/class/midi/midi_device.c
	${TOP}/src/class/msc/msc_device.c
	${TOP}/src/class/net/net_device.c
	${TOP}/src/class/usbtmc/usbtmc_device.c
	${TOP}/src/class/vendor/vendor_device.c
	${TOP}/src/portable/raspberrypi/${FAMILY}/dcd_rp2040.c
	${TOP}/src/portable/raspberrypi/${FAMILY}/rp2040_usb.c
)

target_sources(${PROJECT} PUBLIC
  ${TOP}/hw/bsp/${FAMILY}/family.c
  ${SRC_TINYUSB}
)

target_include_directories(${PROJECT} PUBLIC
  ${TOP}/hw
  ${TOP}/src
  ${TOP}/hw/bsp/${FAMILY}/boards/${BOARD}
)

target_compile_definitions(${PROJECT} PUBLIC
  CFG_TUSB_MCU=OPT_MCU_RP2040
  PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
)

if(DEFINED LOG)
  target_compile_definitions(${PROJECT} PUBLIC CFG_TUSB_DEBUG=${LOG} )
  pico_enable_stdio_uart(${PROJECT} 1)
endif()

if(LOGGER STREQUAL "rtt")
  pico_enable_stdio_uart(${PROJECT} 0)

  target_compile_definitions(${PROJECT} PUBLIC
    LOGGER_RTT
    SEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
  )

  target_sources(${PROJECT} PUBLIC
    ${TOP}/lib/SEGGER_RTT/RTT/SEGGER_RTT.c
  )

  target_include_directories(${PROJECT} PUBLIC
    ${TOP}/lib/SEGGER_RTT/RTT
  )
endif()

