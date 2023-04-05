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

list(APPEND compile_options
    "-DCFG_TUSB_MCU=${tusb_mcu}"
    "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
    #"-DCFG_TUSB_DEBUG=1"
    )

idf_component_get_property(freertos_component_dir freertos COMPONENT_DIR)

list(APPEND includes_public
    "${tusb_src}"
    # The FreeRTOS API include convention in tinyusb is different from esp-idf
    #"${freertos_component_dir}/include/freertos"
    )

list(APPEND srcs
    "${tusb_src}/tusb.c"
    "${tusb_src}/common/tusb_fifo.c"
    "${tusb_src}/device/usbd.c"
    "${tusb_src}/device/usbd_control.c"
    "${tusb_src}/class/cdc/cdc_device.c"
    "${tusb_src}/class/dfu/dfu_rt_device.c"
    "${tusb_src}/class/hid/hid_device.c"
    "${tusb_src}/class/midi/midi_device.c"
    "${tusb_src}/class/msc/msc_device.c"
    "${tusb_src}/class/net/ecm_rndis_device.c"
    "${tusb_src}/class/net/ncm_device.c"
    "${tusb_src}/class/usbtmc/usbtmc_device.c"
    "${tusb_src}/class/vendor/vendor_device.c"
    "${tusb_src}/portable/synopsys/dwc2/dcd_dwc2.c"
    )

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${includes_public}
    REQUIRES src
    )

target_compile_options(${COMPONENT_LIB} PUBLIC ${compile_options})
