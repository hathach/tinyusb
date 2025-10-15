include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

set(SDK_DIR ${TOP}/hw/mcu/sony/cxd56/spresense-exported-sdk)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m4 CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS CXD56 CACHE INTERNAL "")

# Detect platform for mkspk tool
set(PLATFORM ${CMAKE_SYSTEM_NAME})
if(PLATFORM STREQUAL "Darwin")
  set(MKSPK ${TOP}/hw/mcu/sony/cxd56/mkspk/mkspk)
elseif(PLATFORM STREQUAL "Linux")
  set(MKSPK ${TOP}/hw/mcu/sony/cxd56/mkspk/mkspk)
else()
  set(MKSPK ${TOP}/hw/mcu/sony/cxd56/mkspk/mkspk.exe)
endif()

#------------------------------------
# BOARD_TARGET
#------------------------------------
function(add_board_target BOARD_TARGET)
  if (TARGET ${BOARD_TARGET})
    return()
  endif ()

  set(LD_FILE_GNU ${SDK_DIR}/nuttx/scripts/ramconfig.ld)
  set(LD_FILE_Clang ${LD_FILE_GNU})

  if (NOT DEFINED LD_FILE_${CMAKE_C_COMPILER_ID})
    message(FATAL_ERROR "LD_FILE_${CMAKE_C_COMPILER_ID} not defined")
  endif ()

  # Spresense uses NuttX libraries
  add_library(${BOARD_TARGET} INTERFACE)

  target_include_directories(${BOARD_TARGET} INTERFACE
    ${SDK_DIR}/nuttx/include
    ${SDK_DIR}/nuttx/arch
    ${SDK_DIR}/nuttx/arch/chip
    ${SDK_DIR}/nuttx/arch/os
    ${SDK_DIR}/sdk/include
  )

  target_compile_definitions(${BOARD_TARGET} INTERFACE
    CONFIG_HAVE_DOUBLE
    main=spresense_main
  )

  target_compile_options(${BOARD_TARGET} INTERFACE
    -pipe
    -fno-builtin
    -fno-strength-reduce
    -fomit-frame-pointer
    -Wno-error=undef
    -Wno-error=cast-align
    -Wno-error=unused-parameter
    -Wno-error=shadow
    -Wno-error=redundant-decls
  )

  update_board(${BOARD_TARGET})

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${BOARD_TARGET} INTERFACE
      "LINKER:--script=${LD_FILE_GNU}"
      -Xlinker --entry=__start
      -nostartfiles
      -nodefaultlibs
      -Wl,--gc-sections
      -u spresense_main
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${BOARD_TARGET} INTERFACE
      "LINKER:--script=${LD_FILE_Clang}"
      -Xlinker --entry=__start
      -nostartfiles
      -nodefaultlibs
      -u spresense_main
      )
  endif ()
endfunction()


#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  # Board target
  add_board_target(board_${BOARD})

  #---------- Port Specific ----------
  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    )
  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  # Add TinyUSB target and port source
  family_add_tinyusb(${TARGET} OPT_MCU_CXD56)
  target_sources(${TARGET} PUBLIC
    ${TOP}/src/portable/sony/cxd56/dcd_cxd56.c
    )
  target_link_libraries(${TARGET} PUBLIC
    board_${BOARD}
    ${SDK_DIR}/nuttx/libs/libapps.a
    ${SDK_DIR}/nuttx/libs/libnuttx.a
    gcc  # Compiler runtime support for FP operations like __aeabi_dmul
    )

  # Build mkspk tool
  add_custom_command(OUTPUT ${MKSPK}
    COMMAND $(MAKE) -C ${TOP}/hw/mcu/sony/cxd56/mkspk
    COMMENT "Building mkspk tool"
  )

  # Create .spk file
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.spk
    COMMAND ${MKSPK} -c 2 $<TARGET_FILE:${TARGET}> nuttx ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.spk
    DEPENDS ${TARGET} ${MKSPK}
    COMMENT "Creating ${TARGET}.spk"
  )
endfunction()
