cmake_minimum_required(VERSION 3.5)

# use directory name for project id
get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

# Check for -DFAMILY=
if(NOT DEFINED FAMILY)
  message(FATAL_ERROR "Invalid FAMILY specified")
endif()

include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
project(${PROJECT})

project(hid_composite_freertos)
