# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# example src directory
set(EXTRA_COMPONENT_DIRS "src")

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../../..")
get_filename_component(TOP "${TOP}" REALPATH)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(board_test)
