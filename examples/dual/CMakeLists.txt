cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../hw/bsp/family_support.cmake)

project(tinyusb_dual_examples C CXX ASM)
family_initialize_project(tinyusb_dual_examples ${CMAKE_CURRENT_LIST_DIR})

if (FAMILY STREQUAL "rp2040" AND NOT TARGET tinyusb_pico_pio_usb)
  message("Skipping dual host/device mode examples as Pico-PIO-USB is not available")
else ()
  # family_add_subdirectory will filter what to actually add based on selected FAMILY
  set(EXAMPLE_LIST
    host_hid_to_device_cdc
    host_info_to_device_cdc
    )

  foreach (example ${EXAMPLE_LIST})
    family_add_subdirectory(${example})
  endforeach ()
endif ()
