cmake_minimum_required(VERSION 3.17)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
  )

# Example include
target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

# Configure compilation flags and libraries for the example without RTOS.
# See the corresponding function in hw/bsp/FAMILY/family.cmake for details.
family_configure_dual_usb_example(${PROJECT} noos)

# due to warnings from Pico-PIO-USB
target_compile_options(${PROJECT} PUBLIC
  -Wno-error=shadow
  -Wno-error=cast-align
  -Wno-error=cast-qual
  -Wno-error=redundant-decls
  -Wno-error=sign-conversion
  -Wno-error=conversion
  -Wno-error=sign-compare
  -Wno-error=unused-function
  )
