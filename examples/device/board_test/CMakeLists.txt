cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)

# Check for -DFAMILY=
if(FAMILY MATCHES "^esp32s[2-3]")
  # use BOARD-Directory name for project id
  get_filename_component(PROJECT ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  set(PROJECT ${BOARD}-${PROJECT})

  # TOP is absolute path to root directory of TinyUSB git repo
  set(TOP "../../..")
  get_filename_component(TOP "${TOP}" REALPATH)

  project(${PROJECT})

else()

  # gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
  family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

  project(${PROJECT})

  # Checks this example is valid for the family and initializes the project
  family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

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
