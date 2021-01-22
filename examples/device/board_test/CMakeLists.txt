cmake_minimum_required(VERSION 3.5)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

set(PROJECT board_test)

# Check for -DFAMILY=
if(NOT DEFINED FAMILY)
  message(FATAL_ERROR "Invalid FAMILY specified")
endif()

include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
project(${PROJECT})

if(FAMILY STREQUAL "rp2040")

pico_sdk_init()

add_executable(${PROJECT})

target_sources(${PROJECT} PUBLIC
  src/main.c
  "${TOP}/hw/bsp/${FAMILY}/family.c"
)

target_include_directories(${PROJECT} PUBLIC
  "src/"
  "${TOP}/hw"
  "${TOP}/src"
  "${TOP}/hw/bsp/${FAMILY}/boards/${BOARD}"
)

target_compile_definitions(${PROJECT} PUBLIC
  CFG_TUSB_MCU=OPT_MCU_RP2040
  CFG_TUSB_OS=OPT_OS_PICO
)

target_link_libraries(${PROJECT} pico_stdlib)

pico_add_extra_outputs(${PROJECT})

endif()
