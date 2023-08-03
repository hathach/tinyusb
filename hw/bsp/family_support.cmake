include_guard()

include(CMakePrintHelpers)

# TOP is path to root directory
set(TOP "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(TOP ${TOP} ABSOLUTE)

# Default to gcc
if (NOT DEFINED TOOLCHAIN)
  set(TOOLCHAIN gcc)
endif ()

# FAMILY not defined, try to detect it from BOARD
if (NOT DEFINED FAMILY)
  if (NOT DEFINED BOARD)
    message(FATAL_ERROR "You must set a FAMILY variable for the build (e.g. rp2040, espressif).
    You can do this via -DFAMILY=xxx on the cmake command line")
  endif ()

  # Find path contains BOARD
  file(GLOB BOARD_PATH LIST_DIRECTORIES true
    RELATIVE ${TOP}/hw/bsp
    ${TOP}/hw/bsp/*/boards/${BOARD}
    )
  if (NOT BOARD_PATH)
    message(FATAL_ERROR "Could not detect FAMILY from BOARD=${BOARD}")
  endif ()

  # replace / with ; so that we can get the first element as FAMILY
  string(REPLACE "/" ";" BOARD_PATH ${BOARD_PATH})
  list(GET BOARD_PATH 0 FAMILY)
endif ()

if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)
  message(FATAL_ERROR "Family '${FAMILY}' is not known/supported")
endif()

# enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if (IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

set(WARNING_FLAGS_GNU
  -Wall
  -Wextra
  -Werror
  -Wfatal-errors
  -Wdouble-promotion
  -Wstrict-prototypes
  -Wstrict-overflow
  -Werror-implicit-function-declaration
  -Wfloat-equal
  -Wundef
  -Wshadow
  -Wwrite-strings
  -Wsign-compare
  -Wmissing-format-attribute
  -Wunreachable-code
  -Wcast-align
  -Wcast-function-type
  -Wcast-qual
  -Wnull-dereference
  -Wuninitialized
  -Wunused
  -Wreturn-type
  -Wredundant-decls
  )

set(WARNINGS_FLAGS_IAR "")


# Filter example based on only.txt and skip.txt
function(family_filter RESULT DIR)
  get_filename_component(DIR ${DIR} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  if (EXISTS "${DIR}/only.txt")
    file(READ "${DIR}/only.txt" ONLYS)
    # Replace newlines with semicolon so that it is treated as a list by CMake
    string(REPLACE "\n" ";" ONLYS_LINES ${ONLYS})

    # For each mcu
    foreach(MCU IN LISTS FAMILY_MCUS)
      # For each line in only.txt
      foreach(_line ${ONLYS_LINES})
        # If mcu:xxx exists for this mcu or board:xxx then include
        if (${_line} STREQUAL "mcu:${MCU}" OR ${_line} STREQUAL "board:${BOARD}")
          set(${RESULT} 1 PARENT_SCOPE)
          return()
        endif()
      endforeach()
    endforeach()

    # Didn't find it in only file so don't build
    set(${RESULT} 0 PARENT_SCOPE)

  elseif (EXISTS "${DIR}/skip.txt")
    file(READ "${DIR}/skip.txt" SKIPS)
    # Replace newlines with semicolon so that it is treated as a list by CMake
    string(REPLACE "\n" ";" SKIPS_LINES ${SKIPS})

    # For each mcu
    foreach(MCU IN LISTS FAMILY_MCUS)
      # For each line in only.txt
      foreach(_line ${SKIPS_LINES})
        # If mcu:xxx exists for this mcu then skip
        if (${_line} STREQUAL "mcu:${MCU}")
          set(${RESULT} 0 PARENT_SCOPE)
          return()
        endif()
      endforeach()
    endforeach()

    # Didn't find in skip file so build
    set(${RESULT} 1 PARENT_SCOPE)
  else()

    # Didn't find skip or only file so build
    set(${RESULT} 1 PARENT_SCOPE)
  endif()
endfunction()


function(family_add_subdirectory DIR)
  family_filter(SHOULD_ADD "${DIR}")
  if (SHOULD_ADD)
    add_subdirectory(${DIR})
  endif()
endfunction()


function(family_get_project_name OUTPUT_NAME DIR)
  get_filename_component(SHORT_NAME ${DIR} NAME)
  set(${OUTPUT_NAME} ${TINYUSB_FAMILY_PROJECT_NAME_PREFIX}${SHORT_NAME} PARENT_SCOPE)
endfunction()


function(family_initialize_project PROJECT DIR)
  # set output suffix to .elf (skip espressif and rp2040)
  if(NOT FAMILY STREQUAL "espressif" AND NOT FAMILY STREQUAL "rp2040")
    set(CMAKE_EXECUTABLE_SUFFIX .elf PARENT_SCOPE)
  endif()

  family_filter(ALLOWED "${DIR}")
  if (NOT ALLOWED)
    get_filename_component(SHORT_NAME ${DIR} NAME)
    message(FATAL_ERROR "${SHORT_NAME} is not supported on FAMILY=${FAMILY}")
  endif()
endfunction()


#-------------------------------------------------------------
# Common Target Configure
# Most families use these settings except rp2040 and espressif
#-------------------------------------------------------------

# Add RTOS to example
function(family_add_rtos TARGET RTOS)
  if (RTOS STREQUAL "freertos")
    # freertos config
    if (NOT TARGET freertos_config)
      add_library(freertos_config INTERFACE)
      target_include_directories(freertos_config INTERFACE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${FAMILY}/FreeRTOSConfig)
      # add board definition to freertos_config mostly for SystemCoreClock
      target_link_libraries(freertos_config INTERFACE board_${BOARD})
    endif()

    # freertos kernel
    if (NOT TARGET freertos_kernel)
      add_subdirectory(${TOP}/lib/FreeRTOS-Kernel ${CMAKE_BINARY_DIR}/lib/freertos_kernel)
    endif ()

    target_link_libraries(${TARGET} PUBLIC freertos_kernel)
  endif ()
endfunction()


# Add common configuration to example
function(family_configure_common TARGET RTOS)
  family_add_rtos(${TARGET} ${RTOS})

  # run size after build
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    )

  # Add warnings flags
  target_compile_options(${TARGET} PUBLIC ${WARNING_FLAGS_${CMAKE_C_COMPILER_ID}})

  # Generate linker map file
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC "LINKER:-Map=$<TARGET_FILE:${TARGET}>.map")
  endif()

  # ETM Trace option
  if (TRACE_ETM STREQUAL "1")
    target_compile_definitions(${TARGET} PUBLIC TRACE_ETM)
  endif ()

  # LOGGER option
  if (DEFINED LOGGER)
    target_compile_definitions(${TARGET} PUBLIC LOGGER_${LOGGER})

    # Add segger rtt to example
    if(LOGGER STREQUAL "RTT" OR LOGGER STREQUAL "rtt")
      if (NOT TARGET segger_rtt)
        add_library(segger_rtt STATIC ${TOP}/lib/SEGGER_RTT/RTT/SEGGER_RTT.c)
        target_include_directories(segger_rtt PUBLIC ${TOP}/lib/SEGGER_RTT/RTT)
      endif()
      target_link_libraries(${TARGET} PUBLIC segger_rtt)
    endif ()
  endif ()
endfunction()


# Add tinyusb to example
function(family_add_tinyusb TARGET OPT_MCU RTOS)
  # tinyusb target is built for each example since it depends on example's tusb_config.h
  set(TINYUSB_TARGET_PREFIX ${TARGET}-)
  add_library(${TARGET}-tinyusb_config INTERFACE)

  # path to tusb_config.h
  target_include_directories(${TARGET}-tinyusb_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(${TARGET}-tinyusb_config INTERFACE CFG_TUSB_MCU=${OPT_MCU})

  if (DEFINED LOG)
    target_compile_definitions(${TARGET}-tinyusb_config INTERFACE CFG_TUSB_DEBUG=${LOG})
  endif()

  if (RTOS STREQUAL "freertos")
    target_compile_definitions(${TARGET}-tinyusb_config INTERFACE CFG_TUSB_OS=OPT_OS_FREERTOS)
  endif ()

  # tinyusb's CMakeList.txt
  add_subdirectory(${TOP}/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb)

  if (RTOS STREQUAL "freertos")
    # link tinyusb with freeRTOS kernel
    target_link_libraries(${TARGET}-tinyusb PUBLIC freertos_kernel)
  endif ()
endfunction()


# Add bin/hex output
function(family_add_bin_hex TARGET)
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
    VERBATIM)
endfunction()


#----------------------------------
# Example Target Configure (Default rule)
# These function can be redefined in FAMILY/family.cmake
#----------------------------------

function(family_configure_example TARGET RTOS)
  # empty function, should be redefined in FAMILY/family.cmake
endfunction()

# Configure device example with RTOS
function(family_configure_device_example TARGET RTOS)
  family_configure_example(${TARGET} ${RTOS})
endfunction()


# Configure host example with RTOS
function(family_configure_host_example TARGET RTOS)
  family_configure_example(${TARGET} ${RTOS})
endfunction()


# Configure host + device example with RTOS
function(family_configure_dual_usb_example TARGET RTOS)
  family_configure_example(${TARGET} ${RTOS})
endfunction()

#----------------------------------
# RPI specific: refactor later
#----------------------------------
function(family_add_default_example_warnings TARGET)
  target_compile_options(${TARGET} PUBLIC
    -Wall
    -Wextra
    -Werror
    -Wfatal-errors
    -Wdouble-promotion
    -Wfloat-equal
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
    #-Wstrict-prototypes
    #-Werror-implicit-function-declaration
    #-Wundef
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0)
      target_link_options(${TARGET} PUBLIC "LINKER:--no-warn-rwx-segments")
    endif()

    # GCC 10
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
      target_compile_options(${TARGET} PUBLIC -Wconversion)
    endif()

    # GCC 8
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
      target_compile_options(${TARGET} PUBLIC -Wcast-function-type -Wstrict-overflow)
    endif()

    # GCC 6
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
      target_compile_options(${TARGET} PUBLIC -Wno-strict-aliasing)
    endif()
  endif()
endfunction()

#----------------------------------
# Flashing target
#----------------------------------

# Add flash jlink target
function(family_flash_jlink TARGET)
  if (NOT DEFINED JLINKEXE)
    set(JLINKEXE JLinkExe)
  endif ()

  file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.jlink
    CONTENT "halt
loadfile $<TARGET_FILE:${TARGET}>
r
go
exit"
    )

  add_custom_target(${TARGET}-jlink
    DEPENDS ${TARGET}
    COMMAND ${JLINKEXE} -device ${JLINK_DEVICE} -if swd -JTAGConf -1,-1 -speed auto -CommandFile ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.jlink
    )
endfunction()


# Add flash stlink target
function(family_flash_stlink TARGET)
  if (NOT DEFINED STM32_PROGRAMMER_CLI)
    set(STM32_PROGRAMMER_CLI STM32_Programmer_CLI)
  endif ()

  add_custom_target(${TARGET}-stlink
    DEPENDS ${TARGET}
    COMMAND ${STM32_PROGRAMMER_CLI} --connect port=swd --write $<TARGET_FILE:${TARGET}> --go
    )
endfunction()


# Add flash pycod target
function(family_flash_pyocd TARGET)
  if (NOT DEFINED PYOC)
    set(PYOCD pyocd)
  endif ()

  add_custom_target(${TARGET}-pyocd
    DEPENDS ${TARGET}
    COMMAND ${PYOCD} flash -t ${PYOCD_TARGET} $<TARGET_FILE:${TARGET}>
    )
endfunction()


# Add flash using NXP's LinkServer (redserver)
# https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/linkserver-for-microcontrollers:LINKERSERVER
function(family_flash_nxplink TARGET)
  if (NOT DEFINED LINKSERVER)
    set(LINKSERVER LinkServer)
  endif ()

  # LinkServer has a bug that can only execute with full path otherwise it throws:
  # realpath error: No such file or directory
  execute_process(COMMAND which ${LINKSERVER} OUTPUT_VARIABLE LINKSERVER_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)

  add_custom_target(${TARGET}-nxplink
    DEPENDS ${TARGET}
    COMMAND ${LINKSERVER_PATH} flash ${NXPLINK_DEVICE} load $<TARGET_FILE:${TARGET}>
    )
endfunction()


#----------------------------------
# Family specific
#----------------------------------

# family specific: can override above functions
include(${CMAKE_CURRENT_LIST_DIR}/${FAMILY}/family.cmake)

if (NOT FAMILY_MCUS)
  set(FAMILY_MCUS ${FAMILY})
endif()

# save it in case of re-inclusion
set(FAMILY_MCUS ${FAMILY_MCUS} CACHE INTERNAL "")
