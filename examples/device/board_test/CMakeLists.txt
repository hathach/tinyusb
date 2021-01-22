cmake_minimum_required(VERSION 3.5)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

# Check for -DFAMILY=
if(NOT DEFINED FAMILY)
  message(FATAL_ERROR "Invalid FAMILY specified")
endif()

include(${TOP}/hw/bsp/${FAMILY}/family.cmake)

project(board_test)

include(${TOP}/hw/bsp/${FAMILY}/family_extra.cmake OPTIONAL)
