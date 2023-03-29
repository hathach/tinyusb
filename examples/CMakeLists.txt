cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_SOURCE_DIR}/../hw/bsp/family_support.cmake)

project(tinyusb_examples)

add_subdirectory(device)
add_subdirectory(dual)
add_subdirectory(host)
