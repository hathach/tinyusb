cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/${FAMILY}/family.cmake)

# Check for -DFAMILY=
if(FAMILY MATCHES "^esp32s[2-3]")
  # use BOARD-Directory name for project id
  get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  set(PROJECT ${BOARD}-${PROJECT})

  # TOP is absolute path to root directory of TinyUSB git repo
  set(TOP "../../..")
  get_filename_component(TOP "${TOP}" REALPATH)

  include(${TOP}/hw/bsp/${FAMILY}/family.cmake)
  project(${PROJECT})

else()

  # Family chooses name based on passed name and vars (e.g. <BOARD>-<DIR_NAME>)
  get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  family_get_project_name(PROJECT "${DIR_NAME}")

  project(${PROJECT})

  add_executable(${PROJECT})

  # Example source
  target_sources(${PROJECT} PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
          )

  # Example include
  target_include_directories(${PROJECT} PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          )

  # Configure compilation flags and libraries for the example... see the corresponding function
  # in hw/bsp/FAMILY/family.cmake for details.
  family_configure_device_example(${PROJECT})
endif()
