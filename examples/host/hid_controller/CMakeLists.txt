cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT})

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hid_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
        )

# Example include
target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

# Configure compilation flags and libraries for the example... see the corresponding function
# in hw/bsp/FAMILY/family.cmake for details.
family_configure_host_example(${PROJECT})

# For rp2040 enable pico-pio-usb
if(FAMILY STREQUAL "rp2040")
  family_add_pico_pio_usb(${PROJECT})

  # due to warnings from Pico-PIO-USB
  target_compile_options(${PROJECT} PUBLIC
          -Wno-error=cast-qual
          -Wno-error=sign-conversion
          -Wno-error=conversion
          )
endif()