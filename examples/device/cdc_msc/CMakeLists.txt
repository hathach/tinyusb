cmake_minimum_required(VERSION 3.5)

# use directory name for project id
get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

# Check for -DFAMILY=
if(FAMILY STREQUAL "esp32s2")
  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
  project(${PROJECT})

elseif(FAMILY STREQUAL "rp2040")
  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)

  # Example source
  target_sources(${PROJECT} PUBLIC
    src/main.c
    src/msc_disk.c
    src/usb_descriptors.c
  )

  # Example include
  target_include_directories(${PROJECT} PUBLIC
    src
  )

else()
  message(FATAL_ERROR "Invalid FAMILY specified")
endif()
