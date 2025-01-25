cmake_minimum_required(VERSION 3.20)
#set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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

if (RTOS STREQUAL zephyr)
  set(EXE_NAME app)
else()
  set(EXE_NAME ${PROJECT})
  add_executable(${EXE_NAME})
endif()

# Example source
target_sources(${EXE_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/msc_disk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
  )

# Example include
target_include_directories(${EXE_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

# Configure compilation flags and libraries for the example without RTOS.
# See the corresponding function in hw/bsp/FAMILY/family.cmake for details.
family_configure_device_example(${EXE_NAME} ${RTOS})
