cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

project(midi2_host_feather C CXX ASM)

family_initialize_project(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR})

# This example requires PIO-USB and Pico SDK (I2C, SSD1306 display)
if(NOT FAMILY STREQUAL "rp2040")
  return()
endif()

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/display.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

family_configure_host_example(${PROJECT_NAME} noos)

# Adafruit Feather RP2040 USB Host: PIO-USB on GP16/GP17
target_compile_definitions(${PROJECT_NAME} PRIVATE
  PIO_USB_DP_PIN_DEFAULT=16
)
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wno-type-limits
)

# SSD1306 display (I2C via STEMMA QT)
target_link_libraries(${PROJECT_NAME} PUBLIC
  hardware_i2c
)
