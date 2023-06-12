cmake_minimum_required(VERSION 3.17)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../hw/bsp/family_support.cmake)

project(tinyusb_host_examples C CXX ASM)
family_initialize_project(tinyusb_host_examples ${CMAKE_CURRENT_LIST_DIR})

# family_add_subdirectory will filter what to actually add based on selected FAMILY
family_add_subdirectory(power_delivery)
