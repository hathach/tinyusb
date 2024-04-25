cmake_minimum_required(VERSION 3.5)

# Apply board specific content i.e IDF_TARGET must be set before project.cmake is included
include("${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake")

string(TOUPPER ${IDF_TARGET} FAMILY_MCUS)

# Add example src and bsp directories
set(EXTRA_COMPONENT_DIRS "src" "${CMAKE_CURRENT_LIST_DIR}/boards" "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
