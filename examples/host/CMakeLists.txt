cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../hw/bsp/family_support.cmake)

project(tinyusb_host_examples C CXX ASM)
family_initialize_project(tinyusb_host_examples ${CMAKE_CURRENT_LIST_DIR})

# family_add_subdirectory will filter what to actually add based on selected FAMILY
set(EXAMPLE_LIST
  bare_api
  cdc_msc_hid
  cdc_msc_hid_freertos
  device_info
  hid_controller
  midi_rx
  msc_file_explorer
  )

foreach (example ${EXAMPLE_LIST})
  family_add_subdirectory(${example})
endforeach ()
