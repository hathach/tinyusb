cmake_minimum_required(VERSION 3.5)

# TOP is absolute path to root directory of TinyUSB git repo
# needed for esp32sx build. TOOD could be removed later on
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT})

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Check for -DFAMILY=
if(FAMILY MATCHES "^esp32s[2-3]")
else()
  message(FATAL_ERROR "Invalid FAMILY specified: ${FAMILY}")
endif()
