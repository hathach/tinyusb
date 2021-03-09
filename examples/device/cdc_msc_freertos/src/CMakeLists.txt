idf_component_register(SRCS "main.c" "usb_descriptors.c" "msc_disk.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos soc)

string(REGEX MATCH "^(esp32s[2-3])*" chip_target "$ENV{IDF_TARGET}")

if(NOT chip_target)
    message(FATAL_ERROR "Incorrect target: $ENV{IDF_TARGET}" )
else()
    set(chip_family "esp32sx")
endif()

string(TOUPPER ${chip_target} upper_case_target)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_${upper_case_target}"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"
)

target_sources(${COMPONENT_TARGET} PUBLIC
  "${TOP}/src/tusb.c"
  "${TOP}/src/common/tusb_fifo.c"
  "${TOP}/src/device/usbd.c"
  "${TOP}/src/device/usbd_control.c"
  "${TOP}/src/class/cdc/cdc_device.c"
  "${TOP}/src/class/dfu/dfu_rt_device.c"
  "${TOP}/src/class/hid/hid_device.c"
  "${TOP}/src/class/midi/midi_device.c"
  "${TOP}/src/class/msc/msc_device.c"
  "${TOP}/src/class/net/net_device.c"
  "${TOP}/src/class/usbtmc/usbtmc_device.c"
  "${TOP}/src/class/vendor/vendor_device.c"
  "${TOP}/src/portable/espressif/${chip_family}/dcd_${chip_family}.c"
)
