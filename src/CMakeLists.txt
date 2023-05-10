# TODO make tinyusb as library that depends on 'tusb_config' interface that exposes the tusb_config.h file
# This file is WIP and should not used yet

cmake_minimum_required(VERSION 3.17)

# Add tinyusb to a target
function(add_tinyusb TARGET)
  target_sources(${TARGET} PUBLIC
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
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/msc/msc_host.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/class/vendor/vendor_host.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )
  # enable all possible warnings
  target_compile_options(${TARGET} PUBLIC
    )
endfunction()

set(TINYUSB_TARGET "tinyusb")
set(TINYUSB_CONFIG_TARGET "tinyusb_config")

if (DEFINED TINYUSB_TARGET_PREFIX)
  set(TINYUSB_TARGET "${TINYUSB_TARGET_PREFIX}${TINYUSB_TARGET}")
  set(TINYUSB_CONFIG_TARGET "${TINYUSB_TARGET_PREFIX}${TINYUSB_CONFIG_TARGET}")
endif ()

if (DEFINED TINYUSB_TARGET_SUFFIX)
  set(TINYUSB_TARGET "${TINYUSB_TARGET}${TINYUSB_TARGET_SUFFIX}")
  set(TINYUSB_CONFIG_TARGET "${TINYUSB_CONFIG_TARGET}${TINYUSB_TARGET_SUFFIX}")
endif ()

add_library(${TINYUSB_TARGET} STATIC)
add_tinyusb(${TINYUSB_TARGET})

# Link with tinyusb_config target

if (NOT TARGET ${TINYUSB_CONFIG_TARGET})
  message(FATAL_ERROR "${TINYUSB_CONFIG_TARGET} target is not defined")
endif()

target_link_libraries(${TINYUSB_TARGET} PUBLIC
  ${TINYUSB_CONFIG_TARGET}
  )
