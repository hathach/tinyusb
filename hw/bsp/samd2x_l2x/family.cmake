include_guard()

# include board specific
include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

# Determine which SAM family based on board configuration
# SAM_FAMILY should be set by board.cmake (samd21, saml21, or saml22)
if(NOT DEFINED SAM_FAMILY)
  # Default to samd21 if not specified for backward compatibility
  set(SAM_FAMILY samd21)
endif()

set(SDK_DIR ${TOP}/hw/mcu/microchip/${SAM_FAMILY})
set(CMSIS_5 ${TOP}/lib/CMSIS_5)

# toolchain set up
set(CMAKE_SYSTEM_CPU cortex-m0plus CACHE INTERNAL "System Processor")
set(CMAKE_TOOLCHAIN_FILE ${TOP}/examples/build_system/cmake/toolchain/arm_${TOOLCHAIN}.cmake)

set(FAMILY_MCUS SAMD21 SAML2X CACHE INTERNAL "")
set(OPENOCD_OPTION "-f interface/cmsis-dap.cfg -c \"transport select swd\" -f target/at91samdXX.cfg")

#------------------------------------
# Startup & Linker script
#------------------------------------
set(LD_FILE_Clang ${LD_FILE_GNU})
set(STARTUP_FILE_GNU ${SDK_DIR}/gcc/gcc/startup_${SAM_FAMILY}.c)
set(STARTUP_FILE_Clang ${STARTUP_FILE_GNU})

#------------------------------------
# Board Target
#------------------------------------
function(family_add_board BOARD_TARGET)
  # Common sources for all SAM families
  set(COMMON_SOURCES
    ${SDK_DIR}/gcc/system_${SAM_FAMILY}.c
    ${SDK_DIR}/hal/src/hal_atomic.c
    ${SDK_DIR}/hpl/gclk/hpl_gclk.c
  )

  # Family-specific sources
  if(SAM_FAMILY STREQUAL "samd21")
    list(APPEND COMMON_SOURCES
      ${SDK_DIR}/hpl/pm/hpl_pm.c
      ${SDK_DIR}/hpl/sysctrl/hpl_sysctrl.c
    )
  else()
    # SAML21/SAML22
    list(APPEND COMMON_SOURCES
      ${SDK_DIR}/hpl/mclk/hpl_mclk.c
      ${SDK_DIR}/hpl/osc32kctrl/hpl_osc32kctrl.c
      ${SDK_DIR}/hpl/oscctrl/hpl_oscctrl.c
      ${SDK_DIR}/hpl/pm/hpl_pm.c
    )
  endif()

  add_library(${BOARD_TARGET} STATIC ${COMMON_SOURCES})

  target_include_directories(${BOARD_TARGET} PUBLIC
    ${SDK_DIR}
    ${SDK_DIR}/config
    ${SDK_DIR}/include
    ${SDK_DIR}/hal/include
    ${SDK_DIR}/hal/utils/include
    ${SDK_DIR}/hpl/pm
    ${SDK_DIR}/hpl/port
    ${SDK_DIR}/hri
    ${CMSIS_5}/CMSIS/Core/Include
  )

  # Family-specific compile definitions
  if(SAM_FAMILY STREQUAL "samd21")
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      CONF_DFLL_OVERWRITE_CALIBRATION=0
    )
  else()
    # SAML21/SAML22
    target_compile_definitions(${BOARD_TARGET} PUBLIC
      CONF_OSC32K_CALIB_ENABLE=0
      CFG_EXAMPLE_VIDEO_READONLY
    )
  endif()

  update_board(${BOARD_TARGET})
endfunction()

#------------------------------------
# Functions
#------------------------------------
function(family_configure_example TARGET RTOS)
  family_configure_common(${TARGET} ${RTOS})

  # Determine MCU option based on SAM_FAMILY
  if(SAM_FAMILY STREQUAL "samd21")
    set(MCU_OPTION OPT_MCU_SAMD21)
  elseif(SAM_FAMILY STREQUAL "saml21")
    set(MCU_OPTION OPT_MCU_SAML21)
  elseif(SAM_FAMILY STREQUAL "saml22")
    set(MCU_OPTION OPT_MCU_SAML22)
  else()
    message(FATAL_ERROR "Unknown SAM_FAMILY: ${SAM_FAMILY}")
  endif()
  family_add_tinyusb(${TARGET} ${MCU_OPTION})

  target_sources(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/family.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../board.c
    ${TOP}/src/portable/microchip/samd/dcd_samd.c
    ${TOP}/src/portable/microchip/samd/hcd_samd.c
    ${STARTUP_FILE_${CMAKE_C_COMPILER_ID}}
    )
  # Add HCD support for SAMD21 (has host capability)
  if(SAM_FAMILY STREQUAL "samd21")
    target_sources(${TARGET} PUBLIC
      ${TOP}/src/portable/microchip/samd/hcd_samd.c
      )
  endif()

  target_include_directories(${TARGET} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boards/${BOARD}
    )

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_GNU}"
      -nostartfiles
      --specs=nosys.specs --specs=nano.specs
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--script=${LD_FILE_Clang}"
      )
  elseif (CMAKE_C_COMPILER_ID STREQUAL "IAR")
    target_link_options(${TARGET} PUBLIC
      "LINKER:--config=${LD_FILE_IAR}"
      )
  endif ()
  set_source_files_properties(${STARTUP_FILE_${CMAKE_C_COMPILER_ID}} PROPERTIES
    SKIP_LINTING ON
    COMPILE_OPTIONS -w)

  # Flashing
  family_add_bin_hex(${TARGET})
  family_flash_jlink(${TARGET})
  #family_flash_openocd(${TARGET})
endfunction()
