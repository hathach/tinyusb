cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../hw/bsp/family_support.cmake)

project(tinyusb_examples C CXX ASM)

set(EXAMPLES_LIST
  device
  dual
  host
  typec
  )

foreach (example ${EXAMPLES_LIST})
  add_subdirectory(${example})
endforeach ()
