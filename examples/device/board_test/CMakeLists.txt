cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# Need to set Espressif defaults before project() is called
if(FAMILY STREQUAL "espressif")
  list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()

project(board_test C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR})

# SWO trace fixture (src/swo_fixture): stm32f407disco and stm32h743nucleo without RTOS only
if (SWO_FIXTURE AND NOT (BOARD MATCHES "^(stm32f407disco|stm32h743nucleo)$" AND RTOS STREQUAL "noos"))
  message(FATAL_ERROR "SWO_FIXTURE supports stm32f407disco and stm32h743nucleo with RTOS=noos only")
endif ()

# Espressif has its own cmake build system
if(FAMILY STREQUAL "espressif")
  return()
endif()

if (RTOS STREQUAL zephyr)
  set(EXE_NAME app)
else()
  set(EXE_NAME ${PROJECT_NAME})
  add_executable(${EXE_NAME})
endif()

# Example source
target_sources(${EXE_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  )

# Example include
target_include_directories(${EXE_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

if (SWO_FIXTURE)
  target_sources(${EXE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/swo_fixture/swo_fixture.c)
  target_include_directories(${EXE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/swo_fixture)
  target_compile_definitions(${EXE_NAME} PRIVATE SWO_FIXTURE)
endif ()

# Configure compilation flags and libraries for the example without RTOS.
# See the corresponding function in hw/bsp/FAMILY/family.cmake for details.
family_configure_device_example(${EXE_NAME} ${RTOS})
