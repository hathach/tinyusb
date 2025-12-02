cmake_minimum_required(VERSION 3.20)

project(tinyusb_unit_tests LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Command to invoke Ceedling. Supports multi-word commands such as "bundle exec ceedling".
set(CEEDLING_COMMAND "ceedling" CACHE STRING "Command used to invoke Ceedling (Ruby gem).")
separate_arguments(CEEDLING_COMMAND_LIST NATIVE_COMMAND "${CEEDLING_COMMAND}")
if (CEEDLING_COMMAND_LIST STREQUAL "")
  message(FATAL_ERROR "CEEDLING_COMMAND is empty; set it to a valid Ceedling invocation.")
endif ()

list(GET CEEDLING_COMMAND_LIST 0 CEEDLING_LAUNCHER)
find_program(CEEDLING_LAUNCHER_PATH NAMES ${CEEDLING_LAUNCHER})
if (NOT CEEDLING_LAUNCHER_PATH)
  message(FATAL_ERROR "Could not find '${CEEDLING_LAUNCHER}' on PATH; adjust CEEDLING_COMMAND or PATH.")
endif ()
list(REMOVE_AT CEEDLING_COMMAND_LIST 0)
list(INSERT CEEDLING_COMMAND_LIST 0 ${CEEDLING_LAUNCHER_PATH})

set(CEEDLING_WORKDIR ${CMAKE_CURRENT_LIST_DIR})
set(CEEDLING_BUILD_DIR ${CEEDLING_WORKDIR}/_build)

# Helper to add a Ceedling-backed test target that compiles into a real CMake executable.
function(add_ceedling_test TARGET_NAME TEST_SOURCE PRODUCT_SOURCES MOCK_SOURCES)
  set(runner ${CEEDLING_BUILD_DIR}/test/runners/${TARGET_NAME}_runner.c)

  add_custom_target(ceedling_gen_${TARGET_NAME}
    COMMAND ${CEEDLING_COMMAND_LIST} test:${TARGET_NAME}
    WORKING_DIRECTORY ${CEEDLING_WORKDIR}
    BYPRODUCTS ${runner}
    USES_TERMINAL
    COMMENT "Generate Ceedling runner/mocks for ${TARGET_NAME}"
    )

  add_executable(${TARGET_NAME}
    ${TEST_SOURCE}
    ${runner}
    ${MOCK_SOURCES}
    ${CEEDLING_BUILD_DIR}/vendor/unity/src/unity.c
    ${CEEDLING_BUILD_DIR}/vendor/cmock/src/cmock.c
    ${PRODUCT_SOURCES}
    )

  set_source_files_properties(
    ${runner}
    ${MOCK_SOURCES}
    ${CEEDLING_BUILD_DIR}/vendor/unity/src/unity.c
    ${CEEDLING_BUILD_DIR}/vendor/cmock/src/cmock.c
    PROPERTIES GENERATED TRUE
    )

  add_dependencies(${TARGET_NAME} ceedling_gen_${TARGET_NAME})

  target_include_directories(${TARGET_NAME} PRIVATE
    ${CEEDLING_WORKDIR}/test
    ${CEEDLING_WORKDIR}/test/support
    ${CEEDLING_BUILD_DIR}/test/runners
    ${CEEDLING_BUILD_DIR}/test/mocks/${TARGET_NAME}
    ${CEEDLING_BUILD_DIR}/vendor/unity/src
    ${CEEDLING_BUILD_DIR}/vendor/cmock/src
    ${CEEDLING_WORKDIR}/../../src
    ${CEEDLING_WORKDIR}/../../src/common
    ${CEEDLING_WORKDIR}/../../src/device
    ${CEEDLING_WORKDIR}/../../src/class
    ${CEEDLING_WORKDIR}/../../src/class/msc
    ${CEEDLING_WORKDIR}/../../src/host
    ${CEEDLING_WORKDIR}/../../src/typec
    ${CEEDLING_WORKDIR}/../../src/osal
    )

  target_compile_definitions(${TARGET_NAME} PRIVATE _UNITY_TEST_)
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
  add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

# Custom targets to keep plain Ceedling entry-points available.
add_custom_target(ceedling_all
  COMMAND ${CEEDLING_COMMAND_LIST} test:all
  WORKING_DIRECTORY ${CEEDLING_WORKDIR}
  USES_TERMINAL
  COMMENT "Run Ceedling (Unity) unit tests"
  )

add_custom_target(ceedling_clean
  COMMAND ${CEEDLING_COMMAND_LIST} clean
  WORKING_DIRECTORY ${CEEDLING_WORKDIR}
  USES_TERMINAL
  COMMENT "Clean Ceedling build outputs"
  )

add_custom_target(ceedling_clobber
  COMMAND ${CEEDLING_COMMAND_LIST} clobber
  WORKING_DIRECTORY ${CEEDLING_WORKDIR}
  USES_TERMINAL
  COMMENT "Clobber Ceedling build outputs"
  )

# Per-test wiring: mocks are generated under _build/test/mocks/<name>/.
add_ceedling_test(
  test_common_func
  ${CEEDLING_WORKDIR}/test/test_common_func.c
  ""
  ""
  )

add_ceedling_test(
  test_fifo
  ${CEEDLING_WORKDIR}/test/test_fifo.c
  ${CEEDLING_WORKDIR}/../../src/common/tusb_fifo.c
  ""
  )
target_compile_definitions(test_fifo PRIVATE CFG_TUSB_FIFO_ACCESS_FIXED_ADDR_RW32=1)

add_ceedling_test(
  test_usbd
  ${CEEDLING_WORKDIR}/test/device/usbd/test_usbd.c
  "${CEEDLING_WORKDIR}/../../src/tusb.c;${CEEDLING_WORKDIR}/../../src/device/usbd.c;${CEEDLING_WORKDIR}/../../src/device/usbd_control.c;${CEEDLING_WORKDIR}/../../src/common/tusb_fifo.c"
  "${CEEDLING_BUILD_DIR}/test/mocks/test_usbd/mock_dcd.c;${CEEDLING_BUILD_DIR}/test/mocks/test_usbd/mock_msc_device.c"
  )

add_ceedling_test(
  test_msc_device
  ${CEEDLING_WORKDIR}/test/device/msc/test_msc_device.c
  "${CEEDLING_WORKDIR}/../../src/tusb.c;${CEEDLING_WORKDIR}/../../src/device/usbd.c;${CEEDLING_WORKDIR}/../../src/device/usbd_control.c;${CEEDLING_WORKDIR}/../../src/class/msc/msc_device.c;${CEEDLING_WORKDIR}/../../src/common/tusb_fifo.c"
  "${CEEDLING_BUILD_DIR}/test/mocks/test_msc_device/mock_dcd.c"
  )

enable_testing()
