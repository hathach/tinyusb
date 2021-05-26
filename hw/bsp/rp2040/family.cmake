# Board specific define e.g boot stage2
# PICO_DEFAULT_BOOT_STAGE2_FILE must be set before pico_sdk_init()
include(${TOP}/hw/bsp/${FAMILY}/boards/${BOARD}/board.cmake)

pico_sdk_init()

target_link_libraries(${PROJECT}
  pico_stdlib
  pico_bootsel_via_double_reset
  pico_fix_rp2040_usb_device_enumeration
)

pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_uart(${PROJECT} 1)

# TinyUSB Stack source
set(SRC_TINYUSB
	${TOP}/src/tusb.c
	${TOP}/src/common/tusb_fifo.c
	${TOP}/src/device/usbd.c
	${TOP}/src/device/usbd_control.c
	${TOP}/src/class/audio/audio_device.c
	${TOP}/src/class/cdc/cdc_device.c
	${TOP}/src/class/dfu/dfu_device.c
	${TOP}/src/class/dfu/dfu_rt_device.c
	${TOP}/src/class/hid/hid_device.c
	${TOP}/src/class/midi/midi_device.c
	${TOP}/src/class/msc/msc_device.c
	${TOP}/src/class/net/net_device.c
	${TOP}/src/class/usbtmc/usbtmc_device.c
	${TOP}/src/class/vendor/vendor_device.c
	${TOP}/src/host/hub.c
	${TOP}/src/host/usbh.c
	${TOP}/src/host/usbh_control.c
	${TOP}/src/class/cdc/cdc_host.c
	${TOP}/src/class/hid/hid_host.c
	${TOP}/src/class/msc/msc_host.c
	${TOP}/src/portable/raspberrypi/${FAMILY}/rp2040_usb.c
	${TOP}/src/portable/raspberrypi/${FAMILY}/dcd_rp2040.c
	${TOP}/src/portable/raspberrypi/${FAMILY}/hcd_rp2040.c
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
endif()

if(LOGGER STREQUAL "rtt")
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

