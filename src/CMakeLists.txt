# TODO make tinyusb as library that depends on 'tusb_config' interface that exposes the tusb_config.h file
# This file is WIP and should not used yet

cmake_minimum_required(VERSION 3.17)

if (NOT TARGET tinyusb_config)
  message(FATAL_ERROR "tinyusb_config target is not defined")
endif()

add_library(tinyusb STATIC
  ${CMAKE_CURRENT_LIST_DIR}/tusb.c
  ${CMAKE_CURRENT_LIST_DIR}/common/tusb_fifo.c
  ${CMAKE_CURRENT_LIST_DIR}/device/usbd.c
  ${CMAKE_CURRENT_LIST_DIR}/device/usbd_control.c
  ${CMAKE_CURRENT_LIST_DIR}/class/audio/audio_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/cdc/cdc_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/dfu/dfu_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/dfu/dfu_rt_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/hid/hid_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/midi/midi_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/msc/msc_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/net/ecm_rndis_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/net/ncm_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/usbtmc/usbtmc_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/vendor/vendor_device.c
  ${CMAKE_CURRENT_LIST_DIR}/class/video/video_device.c
  )

target_include_directories(tinyusb PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  )

target_link_libraries(tinyusb PUBLIC
  tinyusb_config
  )

function(add_tinyusb TARGET)
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tusb.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/common/tusb_fifo.c
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
    )

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    )

  # enable all possible warnings
  target_compile_options(${TARGET} PUBLIC

    )

endfunction()
