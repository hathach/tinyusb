cmake_minimum_required(VERSION 3.13)
project(tinyusb_cdc_esp32p4)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ tinyusb)

set(PROJECT_SOURCES
    main.c
    usb_descriptors.c
    board_init.c
    ../../../src/portable/espressif/esp32p4/dcd_esp32p4.c
)

set(PROJECT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../../src
)

add_definitions(-DCFG_TUSB_MCU=OPT_MCU_ESP32P4)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDES})
target_link_libraries(${PROJECT_NAME} tinyusb_device)
