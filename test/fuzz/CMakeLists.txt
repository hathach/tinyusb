cmake_minimum_required(VERSION 3.20)

project(tinyusb_fuzz C CXX)

cmake_path(SET TOP NORMALIZE ${CMAKE_CURRENT_LIST_DIR}/../..)
set(TINYUSB_FUZZ_TARGET "" CACHE STRING "Harness to configure: cdc, msc, msc_bot, net or net_ncm; empty for all")

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "fuzz harnesses need clang: configure with CC=clang CXX=clang++")
endif ()

# OSS-Fuzz (LIB_FUZZING_ENGINE defined) owns the engine, sanitizer, coverage and optimisation flags through
# CFLAGS/CXXFLAGS; its trunk clang also gains warnings over time, so -Werror is local only.
if (DEFINED ENV{LIB_FUZZING_ENGINE})
  separate_arguments(FUZZ_ENGINE UNIX_COMMAND "$ENV{LIB_FUZZING_ENGINE}")
  set(FUZZ_OPTIONS "")
else ()
  set(FUZZ_ENGINE -fsanitize=fuzzer,address)
  set(FUZZ_OPTIONS -fsanitize=fuzzer-no-link,address -O1 -g -Werror)
endif ()

# unconditional so a reused build dir never keeps seed tests from an earlier local-mode configure
enable_testing()

set(FUZZ_WARNINGS
  -Wall
  -Wextra
  -Wfatal-errors
  -Wdouble-promotion
  -Wstrict-overflow
  -Wfloat-equal
  -Wundef
  -Wshadow
  -Wwrite-strings
  -Wsign-compare
  -Wmissing-format-attribute
  -Wunreachable-code
  -Wcast-align
  -Wcast-qual
  -Wnull-dereference
  -Wuninitialized
  -Wunused
  -Wredundant-decls
  -Wno-error=unreachable-code
  $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes -Werror=implicit-function-declaration>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>
  )

# target_link_libraries rather than link options: OSS-Fuzz's AFL, honggfuzz and centipede engines are archive paths
function(fuzz_target_common TARGET)
  target_link_libraries(${TARGET} PRIVATE ${FUZZ_ENGINE})
  # sections + gc drop stack paths whose controller hooks the harness lacks (e.g. dcd_edpt_iso_alloc); GNU ld keeps
  # them alive through the sanitizer-coverage tables, lld does not
  target_compile_options(${TARGET} PRIVATE -ffunction-sections -fdata-sections -fno-strict-aliasing ${FUZZ_WARNINGS} ${FUZZ_OPTIONS})
  target_link_options(${TARGET} PRIVATE -fuse-ld=lld -Wl,--gc-sections)
  set_target_properties(${TARGET} PROPERTIES C_STANDARD 99 CXX_STANDARD 17 LINKER_LANGUAGE CXX)
endfunction()

# Replay the seed archive once (local libFuzzer builds only; OSS-Fuzz exercises its own engines)
function(fuzz_add_seed_test NAME)
  cmake_parse_arguments(ARG "" "SEED" "" ${ARGN})
  if (DEFINED ENV{LIB_FUZZING_ENGINE})
    return()
  endif ()
  set(CORPUS ${CMAKE_CURRENT_BINARY_DIR}/corpus/${NAME})
  file(REMOVE_RECURSE ${CORPUS})
  file(MAKE_DIRECTORY ${CORPUS})
  if (ARG_SEED)
    file(ARCHIVE_EXTRACT INPUT ${ARG_SEED} DESTINATION ${CORPUS})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ARG_SEED})
  endif ()
  add_test(NAME ${NAME}_seed_replay COMMAND ${NAME} -runs=0 ${CORPUS})
  set_tests_properties(${NAME}_seed_replay PROPERTIES TIMEOUT 300)
endfunction()

# Device stack harness driven through the synthetic controller in dcd_fuzz.cc
function(fuzz_add_device_harness NAME)
  cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES" ${ARGN})
  set(DIR ${CMAKE_CURRENT_LIST_DIR}/device/${NAME})
  add_executable(${NAME}
    ${TOP}/src/tusb.c
    ${TOP}/src/common/tusb_fifo.c
    ${TOP}/src/device/usbd.c
    ${CMAKE_CURRENT_LIST_DIR}/dcd_fuzz.cc
    ${CMAKE_CURRENT_LIST_DIR}/fuzz.cc
    ${CMAKE_CURRENT_LIST_DIR}/usbd_fuzz.cc
    ${DIR}/src/fuzz.cc
    ${DIR}/src/usb_descriptors.cc
    ${ARG_SOURCES}
    )
  target_include_directories(${NAME} PRIVATE ${DIR}/src ${TOP}/src ${TOP}/test ${ARG_INCLUDES})
  target_compile_definitions(${NAME} PRIVATE OPT_MCU_FUZZ=1 CFG_TUSB_MCU=OPT_MCU_FUZZ _FUZZ)
  fuzz_target_common(${NAME})
endfunction()

if (TINYUSB_FUZZ_TARGET STREQUAL "" OR TINYUSB_FUZZ_TARGET STREQUAL "cdc")
  fuzz_add_device_harness(cdc SOURCES ${TOP}/src/class/cdc/cdc_device.c)
  fuzz_add_seed_test(cdc SEED ${CMAKE_CURRENT_LIST_DIR}/device/cdc/cdc_seed_corpus.zip)
endif ()

if (TINYUSB_FUZZ_TARGET STREQUAL "" OR TINYUSB_FUZZ_TARGET STREQUAL "msc")
  fuzz_add_device_harness(msc SOURCES
    ${TOP}/src/class/msc/msc_device.c
    ${CMAKE_CURRENT_LIST_DIR}/msc_fuzz.cc
    )
  fuzz_add_seed_test(msc SEED ${CMAKE_CURRENT_LIST_DIR}/device/msc/msc_seed_corpus.zip)
endif ()

if (TINYUSB_FUZZ_TARGET STREQUAL "" OR TINYUSB_FUZZ_TARGET STREQUAL "msc_bot")
  set(DIR ${CMAKE_CURRENT_LIST_DIR}/device/msc_bot)
  add_executable(msc_bot ${DIR}/fuzz.c)
  target_include_directories(msc_bot PRIVATE ${DIR} ${TOP}/src)
  fuzz_target_common(msc_bot)
  fuzz_add_seed_test(msc_bot)
endif ()

if (TINYUSB_FUZZ_TARGET STREQUAL "" OR TINYUSB_FUZZ_TARGET STREQUAL "net")
  # lwIP headers only (rndis_reports.c needs netif/ethernet.h); fetched by tools/get_deps.py
  if (NOT EXISTS ${TOP}/lib/lwip/src/include/lwip/init.h)
    message(FATAL_ERROR "lib/lwip is missing: run python3 tools/get_deps.py")
  endif ()
  fuzz_add_device_harness(net
    SOURCES
      ${TOP}/src/class/cdc/cdc_device.c
      ${TOP}/src/class/net/ecm_rndis_device.c
      ${TOP}/lib/networking/rndis_reports.c
      ${CMAKE_CURRENT_LIST_DIR}/net_fuzz.cc
    INCLUDES
      ${TOP}/lib/lwip/src/include
      ${TOP}/lib/networking
    )
  fuzz_add_seed_test(net)
endif ()

# NCM receive validator alone: fuzz.c #includes ncm_device.c
if (TINYUSB_FUZZ_TARGET STREQUAL "" OR TINYUSB_FUZZ_TARGET STREQUAL "net_ncm")
  set(DIR ${CMAKE_CURRENT_LIST_DIR}/device/net_ncm)
  add_executable(net_ncm ${DIR}/fuzz.c)
  target_include_directories(net_ncm PRIVATE ${DIR} ${TOP}/src)
  fuzz_target_common(net_ncm)
  fuzz_add_seed_test(net_ncm SEED ${DIR}/net_ncm_seed_corpus.zip)
endif ()
