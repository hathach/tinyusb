
set(TOP "../../../..")
get_filename_component(TOP "${TOP}" REALPATH)
get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)

idf_component_register (SRCS "main.c" "usb_descriptors.c" "esp_usb.c" "esp_usb_netif_glue.c"
                                "${TOP}/lib/networking/dhserver.c" 
                                "${TOP}/lib/networking/dnserver.c" 
                                "${TOP}/lib/networking/rndis_reports.c"
                                "${TOP}/src/tusb.c"
                                "${TOP}/src/common/tusb_fifo.c"
                                "${TOP}/src/device/usbd.c"
                                "${TOP}/src/device/usbd_control.c"
                                "${TOP}/src/class/audio/audio_device.c"
                                "${TOP}/src/class/cdc/cdc_device.c"
                                "${TOP}/src/class/dfu/dfu_rt_device.c"
                                "${TOP}/src/class/hid/hid_device.c"
                                "${TOP}/src/class/midi/midi_device.c"
                                "${TOP}/src/class/msc/msc_device.c"
                                "${TOP}/src/class/net/net_device.c"
                                "${TOP}/src/class/usbtmc/usbtmc_device.c"
                                "${TOP}/src/class/vendor/vendor_device.c"
                                "${TOP}/src/host/hub.c"
                                "${TOP}/src/host/usbh.c"
                                "${TOP}/src/host/usbh_control.c"
                                "${TOP}/src/class/cdc/cdc_host.c"
                                "${TOP}/src/class/hid/hid_host.c"
                                "${TOP}/src/class/msc/msc_host.c"
                                "${TOP}/src/portable/espressif/esp32sx/dcd_esp32sx.c"
                        INCLUDE_DIRS "." 
                                     $ENV{IDF_PATH}
                                     "${TOP}/hw" "${TOP}/src" 
                                     ${CMAKE_CURRENT_SOURCE_DIR} 
                                     ${TOP}/lib/networking
                        REQUIRES freertos esp_netif lwip nvs_flash esp_http_server)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)
