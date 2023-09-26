idf_build_get_property(target IDF_TARGET)

set(srcs)
set(includes_public)
set(compile_options)
set(tusb_src "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src")

if(target STREQUAL "esp32s3")
  set(tusb_mcu "OPT_MCU_ESP32S3")
elseif(target STREQUAL "esp32s2")
  set(tusb_mcu "OPT_MCU_ESP32S2")
else()
  # CONFIG_TINYUSB dependency has been guaranteed by Kconfig logic,
  # So it's not possible that cmake goes here
  message(FATAL_ERROR "TinyUSB is not support on ${target}.")
  return()
endif()

list(APPEND compile_definitions
  CFG_TUSB_MCU=${tusb_mcu}
  CFG_TUSB_OS=OPT_OS_FREERTOS
  )

list(APPEND srcs
  # common
  ${tusb_src}/tusb.c
  ${tusb_src}/common/tusb_fifo.c
  # device
  ${tusb_src}/device/usbd.c
  ${tusb_src}/device/usbd_control.c
  ${tusb_src}/class/audio/audio_device.c
  ${tusb_src}/class/cdc/cdc_device.c
  ${tusb_src}/class/dfu/dfu_device.c
  ${tusb_src}/class/dfu/dfu_rt_device.c
  ${tusb_src}/class/hid/hid_device.c
  ${tusb_src}/class/midi/midi_device.c
  ${tusb_src}/class/msc/msc_device.c
  ${tusb_src}/class/net/ecm_rndis_device.c
  ${tusb_src}/class/net/ncm_device.c
  ${tusb_src}/class/usbtmc/usbtmc_device.c
  ${tusb_src}/class/vendor/vendor_device.c
  ${tusb_src}/class/video/video_device.c
  ${tusb_src}/portable/synopsys/dwc2/dcd_dwc2.c
  # host
  ${tusb_src}/host/usbh.c
  ${tusb_src}/host/hub.c
  ${tusb_src}/class/cdc/cdc_host.c
  ${tusb_src}/class/hid/hid_host.c
  ${tusb_src}/class/msc/msc_host.c
  ${tusb_src}/class/vendor/vendor_host.c
  )

# use max3421 as host controller
if (MAX3421_HOST STREQUAL "1")
  list(APPEND srcs ${tusb_src}/portable/analog/max3421/hcd_max3421.c)
  list(APPEND compile_definitions CFG_TUH_MAX3421=1)
endif ()

if (DEFINED LOG)
  list(APPEND compile_definitions CFG_TUSB_DEBUG=${LOG})
  if (LOG STREQUAL "4")
    # no inline for debug level 4
    list(APPEND compile_definitions TU_ATTR_ALWAYS_INLINE=)
  endif ()
endif()

idf_component_register(SRCS ${srcs}
  INCLUDE_DIRS ${tusb_src}
  REQUIRES src
  )

target_compile_definitions(${COMPONENT_LIB} PUBLIC ${compile_definitions})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
