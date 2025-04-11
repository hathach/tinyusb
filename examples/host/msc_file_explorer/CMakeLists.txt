cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Espressif has its own cmake build system
if(FAMILY STREQUAL "espressif")
  return()
endif()

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/msc_app.c
  ${TOP}/lib/fatfs/source/ff.c
  ${TOP}/lib/fatfs/source/ffsystem.c
  ${TOP}/lib/fatfs/source/ffunicode.c
  )

# Suppress warnings on fatfs
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(
    ${TOP}/lib/fatfs/source/ff.c
    PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-cast-qual"
    )
endif ()

# Example include
target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${TOP}/lib/fatfs/source
  ${TOP}/lib/embedded-cli
  )

# Configure compilation flags and libraries for the example without RTOS.
# See the corresponding function in hw/bsp/FAMILY/family.cmake for details.
family_configure_host_example(${PROJECT} noos)
