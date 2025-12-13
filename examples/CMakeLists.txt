cmake_minimum_required(VERSION 3.20)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/../hw/bsp/family_support.cmake)

project(tinyusb_examples C CXX ASM)

set(EXAMPLES_LIST
  device
  dual
  host
  typec
  )
set(MAPJSON_PATTERNS "")

foreach (example ${EXAMPLES_LIST})
  add_subdirectory(${example})
  list(APPEND MAPJSON_PATTERNS "${CMAKE_BINARY_DIR}/${example}/*/*.map.json")
endforeach ()

# Post-build: run metrics.py on all map.json files
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(tinyusb_metrics
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/metrics.py
  combine -f tinyusb/src -j -o ${CMAKE_BINARY_DIR}/metrics
  ${MAPJSON_PATTERNS}
  COMMENT "Generating average code size metrics"
  VERBATIM
  )
