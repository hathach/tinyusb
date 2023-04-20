cmake_minimum_required(VERSION 3.13)
include(CMakePrintHelpers)

# default toolchain is gcc
if (NOT TOOLCHAIN)
  set(TOOLCHAIN "gcc")
endif ()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Other family such as rp2040
if(NOT FAMILY STREQUAL "espressif")
  add_executable(${PROJECT})

  # Example source
  target_sources(${PROJECT} PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
          )

  # Example include
  target_include_directories(${PROJECT} PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          )

  if(FAMILY STREQUAL "rp2040")
    # Configure compilation flags and libraries for the example... see the corresponding function
    # in hw/bsp/FAMILY/family.cmake for details.
    family_configure_device_example(${PROJECT})
  else ()
    # TOP is absolute path to root directory of TinyUSB git repo
    set(TOP "${CMAKE_CURRENT_LIST_DIR}/../../..")
    get_filename_component(TOP "${TOP}" REALPATH)

    # re-include family.cmake
    include(${TOP}/hw/bsp/${FAMILY}/family.cmake)

    target_sources(${PROJECT} PUBLIC
      ${TOP}/src/device/usbd.c
      ${TOP}/src/device/usbd_control.c
      ${TOP}/src/class/audio/audio_device.c
      ${TOP}/src/class/cdc/cdc_device.c
      ${TOP}/src/class/dfu/dfu_device.c
      ${TOP}/src/class/dfu/dfu_rt_device.c
      ${TOP}/src/class/hid/hid_device.c
      ${TOP}/src/class/midi/midi_device.c
      ${TOP}/src/class/msc/msc_device.c
      ${TOP}/src/class/net/ecm_rndis_device.c
      ${TOP}/src/class/net/ncm_device.c
      ${TOP}/src/class/usbtmc/usbtmc_device.c
      ${TOP}/src/class/vendor/vendor_device.c
      ${TOP}/src/class/video/video_device.c
      )

    target_include_directories(${PROJECT} PUBLIC
      ${TOP}/hw
      ${TOP}/src
      )


  endif ()
endif()
