# TODO more docs and example on how to use this file
# TINYUSB_TARGET_PREFIX and TINYUSB_TARGET_SUFFIX can be used to change the name of the target

cmake_minimum_required(VERSION 3.20)

# Add tinyusb to a existing target
function(tinyusb_target_add TARGET)
  target_sources(${TARGET} PRIVATE
    # common
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tusb.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/common/tusb_fifo.c
    # device
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/device/usbd.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/device/usbd_control.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/audio/audio_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/cdc/cdc_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/dfu/dfu_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/dfu/dfu_rt_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/hid/hid_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/midi/midi_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/msc/msc_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/net/ecm_rndis_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/net/ncm_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/usbtmc/usbtmc_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/vendor/vendor_device.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/video/video_device.c
    # host
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/host/usbh.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/host/hub.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/cdc/cdc_host.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/hid/hid_host.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/midi/midi_host.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/msc/msc_host.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/vendor/vendor_host.c
    # typec
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/typec/usbc.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    # TODO for net driver, should be removed/changed
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../lib/networking
    )
endfunction()
