cmake_minimum_required(VERSION 3.5)

# use BOARD-Directory name for project id
get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJECT ${BOARD}-${PROJECT})

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

# Check for -DFAMILY=
if(FAMILY MATCHES "^esp32s[2-3]")
  cmake_minimum_required(VERSION 3.5)

  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
  project(${PROJECT})

else()
  message(FATAL_ERROR "Invalid FAMILY specified: ${FAMILY}")
endif()
