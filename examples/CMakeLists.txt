cmake_minimum_required(VERSION 3.17)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/../hw/bsp/family_support.cmake)

project(tinyusb_examples C CXX ASM)

add_subdirectory(device)
add_subdirectory(dual)
add_subdirectory(host)
add_subdirectory(typec)
